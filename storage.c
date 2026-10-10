#include "storage.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void init_page(DiskPage *page) {
  PageHeader *header = (PageHeader *)page->data;
  header->item_count = 0;
  // Free space pointer starts at the end of the file
  header->free_space_pointer = PAGE_SIZE;
}

int insert_record_into_page(DiskPage *page, const uint8_t *record_data,
                            uint16_t length) {
  PageHeader *header = (PageHeader *)page->data;

  // End of slot directory
  size_t header_size = sizeof(PageHeader);
  size_t slot_end = header_size + (header->item_count * sizeof(Slot));

  // Space required
  size_t space_needed = sizeof(Slot) + length;
  size_t current_free_space = header->free_space_pointer - slot_end;

  if (current_free_space < space_needed)
    return -1; // Not enough space

  // Updating free space pointer
  header->free_space_pointer -= length;
  uint16_t new_offset = header->free_space_pointer;

  // Inserting data
  memcpy(page->data + new_offset, record_data, length);

  // Updating slots
  Slot *slots = (Slot *)(page->data + header_size);
  uint16_t slot_id = header->item_count;

  slots[slot_id].offset = new_offset;
  slots[slot_id].length = length;

  // Increment item count
  header->item_count++;

  return slot_id; // slot_id to be stored w/ page_id as index point
}

int write_page_to_disk(const char *filename, int page_id,
                       const DiskPage *page) {
  FILE *file = fopen(filename, "r+b");
  if (!file) {
    file = fopen(filename, "w+b");
    if (!file) {
      perror("[-] Error opening the file for writing!");
      return -1;
    }
  }

  // Seeking the page's byte offset
  long offset = (long)page_id * PAGE_SIZE;
  if (fseek(file, offset, SEEK_SET)) {
    perror("[-] Error seeking file!");
    fclose(file);
    return -1;
  }

  // Writing page data
  size_t written = fwrite(page, sizeof(DiskPage), 1, file);
  fclose(file);
  if (written != 1) {
    printf("[-] Error: Failed to write complete page to disk.\n");
    return -1;
  }

  return 0;
}

int read_page_from_disk(const char *filename, int page_id, DiskPage *page) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    perror("[-] Error opening file for reading!");
    return -1;
  }

  // Seeking the page's byte offset
  long offset = (long)page_id * PAGE_SIZE;
  if (fseek(file, offset, SEEK_SET)) {
    perror("[-] Error seeking file!");
    fclose(file);
    return -1;
  }

  // Reading page data
  size_t read_count = fread(page, sizeof(DiskPage), 1, file);
  fclose(file);

  if (read_count != 1)
    return -1;

  return 0;
}

void init_index(IndexTable *idx) { idx->count = 0; }

int index_insert(IndexTable *idx, const char *key, int page_id, int slot_id) {
  if (idx->count >= MAX_INDEX_ENTRIES) {
    printf("[-] Error: Index table is full.\n");
    return -1;
  }

  // Checking if key already exists
  for (int i = 0; i < idx->count; i++) {
    if (!strcmp(idx->entries[i].key, key)) {
      // Updating existing entry location
      idx->entries[i].page_id = page_id;
      idx->entries[i].slot_id = slot_id;
      return 0;
    }
  }

  // Adding the new entry
  strncpy(idx->entries[idx->count].key, key, MAX_KEY_LEN - 1);
  idx->entries[idx->count].key[MAX_KEY_LEN - 1] = '\0';
  idx->entries[idx->count].page_id = page_id;
  idx->entries[idx->count].slot_id = slot_id;
  idx->count++;

  return 0;
}

IndexEntry *index_lookup(IndexTable *idx, const char *key) {
  for (int i = 0; i < idx->count; i++) {
    if (!strcmp(idx->entries[i].key, key)) {
      return &idx->entries[i];
    }
  }

  return NULL;
}

int db_insert(const char *db_filename, DiskPage *active_page,
              int *current_page_id, IndexTable *idx, const char *key,
              const char *value) {
  char payload[512];
  int payload_len = snprintf(payload, sizeof(payload), "%s|%s", key, value) + 1;

  if (payload_len > PAGE_SIZE - sizeof(PageHeader)) {
    printf("[-] Error: Record is too large to ever fit in a single page!\n");
    return -1;
  }

  // Trying to insert into current page
  int slot_id = insert_record_into_page(active_page, (const uint8_t *)payload,
                                        payload_len);

  // If current page is full
  if (slot_id < 0) {
    printf("[*] Page %d is full. Flushing to disk and allocating Page %d...\n",
           *current_page_id, *current_page_id + 1);
    // Writing the page to the disk
    if (write_page_to_disk(db_filename, *current_page_id, active_page))
      return -1;

    // Creating a new page
    (*current_page_id)++;
    init_page(active_page);

    // Inserting into the new page
    slot_id = insert_record_into_page(active_page, (const uint8_t *)payload,
                                      payload_len);
    if (slot_id < 0) {
      printf(
          "[-] Critical Error: Record failed to fit even on a fresh page.\n");
      return -1;
    }
  }

  // Registering the key into index table
  index_insert(idx, key, *current_page_id, slot_id);
  return 0;
}