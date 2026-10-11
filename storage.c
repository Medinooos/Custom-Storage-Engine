#include "storage.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define WAL_MAGIC 0xDEADBEEF

const char *wal_file = "database.wal";

int db_insert(const char *db_filename, FreeSpaceMap *fsm, IndexTable *idx,
              const char *key, const char *value) {
  if (WAL_append(wal_file, WAL_OP_INSERT, key, value))
    return -1;

  return db_insert_internal(db_filename, fsm, idx, key, value);
}

int db_delete(const char *db_filename, FreeSpaceMap *fsm, IndexTable *idx,
              const char *key) {
  if (WAL_append(wal_file, WAL_OP_DELETE, key, NULL))
    return -1;

  return db_delete_internal(db_filename, fsm, idx, key);
}

int db_update(const char *db_filename, FreeSpaceMap *fsm, IndexTable *idx,
              const char *key, const char *new_value) {
  if (WAL_append(wal_file, WAL_OP_INSERT, key, new_value))
    return -1;

  return db_insert_internal(db_filename, fsm, idx, key, new_value);
}

void init_page(DiskPage *page) {
  PageHeader *header = (PageHeader *)page->data;
  header->item_count = 0;
  // Free space pointer starts at the end of the file
  header->free_space_pointer = PAGE_SIZE;
}

void init_index(IndexTable *idx) { idx->count = 0; }

void init_fsm(FreeSpaceMap *fsm) {
  fsm->last_page_id = 0;
  for (int i = 0; i < MAX_PAGES; i++) {
    fsm->free_bytes[i] = PAGE_SIZE - sizeof(PageHeader);
  }
}

void compact_page(DiskPage *page) {
  PageHeader *header = (PageHeader *)page->data;
  Slot *slots = (Slot *)(page->data + sizeof(PageHeader));

  // Temp buffer for compacted data
  char temp_data[PAGE_SIZE];
  uint16_t new_free_space = PAGE_SIZE;

  // Iterating all the slots
  for (uint16_t i = 0; i < header->item_count; i++) {
    if (slots[0].length > 0) { // Non-Dead slots
      char *src = page->data + slots[i].offset;

      // Storing the data into the temp buffer
      new_free_space -= slots[i].length;
      memcpy(temp_data + new_free_space, src, slots[i].length);

      // Updating the slot's offset
      slots[i].offset = new_free_space;
    }
  }

  // Copying the compacted data back to original page
  size_t data_to_copy = PAGE_SIZE - new_free_space;
  memcpy(page->data + new_free_space, temp_data + new_free_space, data_to_copy);

  // Updating the free space pointer
  header->free_space_pointer = new_free_space;
}

int insert_record_into_page(DiskPage *page, const uint8_t *record_data,
                            uint16_t length) {
  PageHeader *header = (PageHeader *)page->data;
  size_t header_size = sizeof(PageHeader);
  Slot *slots = (Slot *)(page->data + sizeof(PageHeader));

  // Slot recycling
  int target_slot_id = -1;
  for (uint16_t i = 0; i < header->item_count; i++) {
    if (!slots[i].length) { // Dead slot found
      target_slot_id = i;
      break;
    }
  }

  size_t space_needed = length;
  if (target_slot_id == -1)
    space_needed += sizeof(Slot); // In case we're using a new slot

  // Size of the contiguous free space
  size_t slots_end = header_size + (header->item_count * sizeof(Slot));
  size_t current_free_space = header->free_space_pointer - slots_end;

  // Checking whether compaction helps if space_needed isn't enough
  if (current_free_space < space_needed) {
    // Would-be size after compaction
    size_t total_live_data_bytes = 0;
    for (uint16_t i = 0; i < header->item_count; i++)
      total_live_data_bytes += slots[i].length;

    size_t potential_free_space = PAGE_SIZE - slots_end - total_live_data_bytes;

    if (potential_free_space <
        space_needed) // Page doesn;t fit, even after compaction
      return -1;

    compact_page(page);
  }

  // Updating free space pointer
  header->free_space_pointer -= length;
  uint16_t new_offset = header->free_space_pointer;

  // Inserting data
  memcpy(page->data + new_offset, record_data, length);

  if (target_slot_id != -1) {
    slots[target_slot_id].offset = new_offset;
    slots[target_slot_id].length = length;
    return target_slot_id;
  } else {
    uint16_t new_slot_id = header->item_count;
    slots[new_slot_id].offset = new_offset;
    slots[new_slot_id].length = length;
    header->item_count++;
    return new_slot_id;
  }
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

int db_insert_large(const char *db_filename, FreeSpaceMap *fsm, IndexTable *idx,
                    const char *key, const uint8_t *data, size_t total_len) {
  size_t bytes_written = 0;
  int first_page_id = -1;
  int first_slot_id = -1;

  int prev_page_id = -1;
  int prev_slot_id = -1;

  // Allocating new pages beyond the current max page in the FSM
  int current_chunk_page = fsm->last_page_id + 1;
  if (current_chunk_page >= MAX_PAGES) {
    printf("[-] Error: Max page limit reached for large insert.\n");
    return -1;
  }

  DiskPage active_page;
  init_page(&active_page);

  char fragment_buf[PAGE_SIZE];

  while (bytes_written < total_len) {
    PageHeader *hdr = (PageHeader *)active_page.data;
    size_t slots_end = sizeof(PageHeader) + (hdr->item_count * sizeof(Slot));
    size_t available_data_space = hdr->free_space_pointer - slots_end;

    // Minimum space needed for slot + fragment header + at least 1 byte of data
    size_t min_needed = sizeof(Slot) + sizeof(RecordFragmentHeader) + 1;
    if (available_data_space < min_needed) {
      // Flush current full page to disk
      write_page_to_disk(db_filename, current_chunk_page, &active_page);

      // Advance to next page
      current_chunk_page++;
      if (current_chunk_page >= MAX_PAGES) {
        printf("[-] Error: Max page limit reached during chaining.\n");
        return -1;
      }
      init_page(&active_page);

      hdr = (PageHeader *)active_page.data;
      slots_end = sizeof(PageHeader) + (hdr->item_count * sizeof(Slot));
      available_data_space = hdr->free_space_pointer - slots_end;
    }

    // Calculating max payload for this chunk
    size_t max_payload_here =
        available_data_space - sizeof(Slot) - sizeof(RecordFragmentHeader);
    size_t bytes_to_write_now = (total_len - bytes_written < max_payload_here)
                                    ? (total_len - bytes_written)
                                    : max_payload_here;

    // Constructing fragment header
    RecordFragmentHeader frag_hdr;
    frag_hdr.next_page_id = -1;
    frag_hdr.next_slot_id = -1;
    frag_hdr.chunk_len = (uint16_t)bytes_to_write_now;

    // Packing header + data payload into fragment buffer
    memcpy(fragment_buf, &frag_hdr, sizeof(RecordFragmentHeader));
    memcpy(fragment_buf + sizeof(RecordFragmentHeader), data + bytes_written,
           bytes_to_write_now);
    size_t total_fragment_size =
        sizeof(RecordFragmentHeader) + bytes_to_write_now;

    // Inserting fragment into page
    int slot_id =
        insert_record_into_page(&active_page, (const uint8_t *)fragment_buf,
                                (uint16_t)total_fragment_size);
    if (slot_id < 0) {
      return -1;
    }

    // Calculating the exact physical space consumed in this page
    size_t total_consumed = total_fragment_size + sizeof(Slot);
    fsm->free_bytes[current_chunk_page] -= (uint16_t)total_consumed;

    if (first_slot_id == -1) {
      first_page_id = current_chunk_page;
      first_slot_id = slot_id;
    } else {
      // Back-patching the previous fragment's next pointers on disk
      DiskPage prev_page;
      read_page_from_disk(db_filename, prev_page_id, &prev_page);
      Slot *p_slots = (Slot *)(prev_page.data + sizeof(PageHeader));
      RecordFragmentHeader *p_frag =
          (RecordFragmentHeader *)(prev_page.data +
                                   p_slots[prev_slot_id].offset);
      p_frag->next_page_id = current_chunk_page;
      p_frag->next_slot_id = slot_id;
      write_page_to_disk(db_filename, prev_page_id, &prev_page);
    }

    prev_page_id = current_chunk_page;
    prev_slot_id = slot_id;
    bytes_written += bytes_to_write_now;
  }

  // Writing the final active page to disk
  write_page_to_disk(db_filename, current_chunk_page, &active_page);

  // Updating FSM's last_page_id
  if (current_chunk_page > fsm->last_page_id) {
    fsm->last_page_id = current_chunk_page;
  }

  // Registering starting point of the massive record in the index table
  index_insert(idx, key, first_page_id, first_slot_id);
  return 0;
}

int db_get(const char *db_filename, IndexTable *idx, const char *key,
           char *output_buffer, size_t max_out_len) {
  IndexEntry *entry = index_lookup(idx, key);
  if (!entry)
    return -1;

  DiskPage page;
  if (read_page_from_disk(db_filename, entry->page_id, &page) != 0)
    return -1;

  Slot *slots = (Slot *)(page.data + sizeof(PageHeader));
  char *record_ptr = page.data + slots[entry->slot_id].offset;
  uint16_t record_len = slots[entry->slot_id].length;

  // Checking if this record is a chained large record or a single-page record.
  int32_t possible_next_page;
  memcpy(&possible_next_page, record_ptr, sizeof(int32_t));

  // Checking if next_page_id is a valid page index or -1.
  if (possible_next_page == -1 ||
      (possible_next_page >= 0 && possible_next_page < MAX_PAGES)) {
    // Retrieving a large record
    int curr_page = entry->page_id;
    int curr_slot = entry->slot_id;
    size_t total_copied = 0;

    while (curr_page != -1 && curr_slot != -1) {
      if (read_page_from_disk(db_filename, curr_page, &page))
        return -1;

      Slot *p_slots = (Slot *)(page.data + sizeof(PageHeader));
      RecordFragmentHeader *frag =
          (RecordFragmentHeader *)(page.data + p_slots[curr_slot].offset);

      if (total_copied + frag->chunk_len >= max_out_len)
        return -1; // Output buffer too small

      char *chunk_data_ptr =
          page.data + p_slots[curr_slot].offset + sizeof(RecordFragmentHeader);
      memcpy(output_buffer + total_copied, chunk_data_ptr, frag->chunk_len);
      total_copied += frag->chunk_len;

      curr_page = frag->next_page_id;
      curr_slot = frag->next_slot_id;
    }

    output_buffer[total_copied] = '\0';
    return (int)total_copied;
  } else {
    // Retrieving a single page record
    char *pipe = memchr(record_ptr, '|', record_len);
    if (!pipe)
      return -1;

    char *value_ptr = pipe + 1;
    size_t value_len = record_len - (value_ptr - record_ptr);

    if (value_len >= max_out_len)
      return -1;

    memcpy(output_buffer, value_ptr, value_len);
    output_buffer[value_len] = '\0';
    return (int)value_len;
  }
}

int fsm_find_page(FreeSpaceMap *fsm, size_t space_needed) {
  for (int i = 0; i <= fsm->last_page_id; i++) {
    if (fsm->free_bytes[i] >= space_needed)
      return -i;
  }
  return -1;
}

int db_insert_internal(const char *db_filename, FreeSpaceMap *fsm,
                       IndexTable *idx, const char *key, const char *value) {
  char payload[512];
  int payload_len = snprintf(payload, sizeof(payload), "%s|%s", key, value) + 1;

  // Checking whether the record is too large for a single page
  size_t max_single_page_capacity =
      PAGE_SIZE - sizeof(PageHeader) - sizeof(Slot);

  if (payload_len > max_single_page_capacity)
    return db_insert_large(db_filename, fsm, idx, key, (const uint8_t *)value,
                           strlen(value) + 1);

  // Checking whether any page has enough space
  int target_page_id = fsm_find_page(fsm, payload_len + sizeof(Slot));
  DiskPage target_page;

  if (target_page_id != -1) {
    if (read_page_from_disk(db_filename, target_page_id, &target_page))
      init_page(&target_page);
  } else {
    // If no page has enough space, allocating a new page at the end
    target_page_id = fsm->last_page_id + 1;
    if (target_page_id >= MAX_PAGES) {
      printf("[-] Error: Database is full, limit reached.\n");
      return -1;
    }
    init_page(&target_page);
    fsm->last_page_id = target_page_id;
    printf("[*] Allocated new page %d", target_page_id);
  }

  // Checking for dead slots to recycle
  PageHeader *hdr = (PageHeader *)target_page.data;
  Slot *slots = (Slot *)(target_page.data + sizeof(PageHeader));
  int dead_slot = -1;
  for (int i = 0; i < hdr->item_count; i++) {
    if (slots[i].length == 0) {
      dead_slot = i;
      break;
    }
  }

  size_t total_consumed = payload_len + (dead_slot != -1 ? 0 : sizeof(Slot));

  // Inserting into target page
  int slot_id = insert_record_into_page(&target_page, (const uint8_t *)payload,
                                        payload_len);
  if (slot_id < 0)
    return -1;

  // Updating FSM after insertion
  fsm->free_bytes[target_page_id] -= total_consumed;

  // Write to disk and registering the key in the index table
  write_page_to_disk(db_filename, target_page_id, &target_page);
  index_insert(idx, key, target_page_id, slot_id);
  return 0;
}

int db_delete_internal(const char *db_filename, FreeSpaceMap *fsm,
                       IndexTable *idx, const char *key) {
  // Looking up the key in the index
  IndexEntry *entry = index_lookup(idx, key);
  if (!entry) // Key not found
    return -1;

  int page_id = entry->page_id;
  int slot_id = entry->slot_id;

  // Loading the page from disk
  DiskPage page;
  if (read_page_from_disk(db_filename, page_id, &page))
    return -1;

  // Marking the slot w/ 0
  Slot *slots = (Slot *)(page.data + sizeof(PageHeader));
  uint16_t deleted_len = slots[slot_id].length;
  slots[slot_id].length = 0;
  slots[slot_id].offset = 0;

  // Returning the modified page back to disk
  if (write_page_to_disk(db_filename, page_id, &page))
    return -1;

  // Updating the FSM
  fsm->free_bytes[page_id] += deleted_len;

  // Removing the entry from the IndexTable
  int index_idx = -1;
  for (int i = 0; i < idx->count; i++) {
    if (!strcmp(idx->entries[i].key, key)) {
      index_idx = i;
      break;
    }
  }

  if (index_idx != -1) {
    for (int i = index_idx; i < idx->count - 1; i++)
      idx->entries[i] = idx->entries[i + 1];
    idx->count--;
  }

  return 0;
}

int db_update_internal(const char *db_filename, FreeSpaceMap *fsm,
                       IndexTable *idx, const char *key,
                       const char *new_value) {
  // Looking up the entry
  IndexEntry *entry = index_lookup(idx, key);
  if (!entry)
    return -1;

  // Formatting new payload string
  char payload[512];
  int new_payload_len =
      snprintf(payload, sizeof(payload), "%s|%s", key, new_value) + 1;

  // Loading the target page
  DiskPage page;
  if (read_page_from_disk(db_filename, entry->page_id, &page))
    return -1;

  Slot *slots = (Slot *)(page.data + sizeof(PageHeader));
  uint16_t old_len = slots[entry->slot_id].length;

  // Conditional patch check
  if (new_payload_len <= old_len) {
    // In-Place update
    memcpy(page.data + slots[entry->slot_id].offset, payload, new_payload_len);

    // Calculating net change in free space
    int diff = old_len - new_payload_len;

    // Updating slot length
    slots[entry->slot_id].length = (uint16_t)new_payload_len;

    // Writing updated page back to disk
    if (write_page_to_disk(db_filename, entry->page_id, &page))
      return -1;

    // FSM adjustment
    if (diff > 0)
      fsm->free_bytes[entry->page_id] += diff;

    return 0;
  } else {
    // Deleting and Re-Inserting
    if (db_delete_internal(db_filename, fsm, idx, key))
      return -1;
    return db_insert(db_filename, fsm, idx, key, new_value);
  }
}

int WAL_append(const char *wal_filename, WALOpType op, const char *key,
               const char *value) {
  FILE *f = fopen(wal_filename, "ab"); // Append in binary mode
  if (!f)
    return -1;

  uint16_t key_len = (uint16_t)strlen(key) + 1;
  uint16_t val_len = value ? (uint16_t)strlen(value) + 1 : 0;

  WALRecordHeader hdr;
  hdr.magic = WAL_MAGIC;
  hdr.op_type = (uint8_t)op;
  hdr.key_len = key_len;
  hdr.val_len = val_len;

  // Writing header to WAL
  fwrite(&hdr, sizeof(WALRecordHeader), 1, f);

  // Writing key to WAL
  fwrite(key, 1, key_len, f);

  // Writing value to WAL
  if (value && val_len > 0)
    fwrite(value, 1, val_len, f);

  fflush(f);
  fsync(fileno(f));
  fclose(f);
  return 0;
}

int recover_from_wal(const char *wal_filename, const char *db_filename,
                     FreeSpaceMap *fsm, IndexTable *idx) {
  FILE *f = fopen(wal_filename, "rb");
  if (!f)
    return 0;

  printf("[*] WAL detected. Starting crash recovery replay...\n");

  WALRecordHeader hdr;
  while (fread(&hdr, sizeof(WALRecordHeader), 1, f) == 1) {
    // Validating magic number to protect against partial/corrupted trailing
    // writes
    if (hdr.magic != WAL_MAGIC) {
      printf("[!] Warning: WAL corruption detected. Stopping replay.\n");
      break;
    }

    char key_buf[256];
    char val_buf[512];

    if (hdr.key_len > sizeof(key_buf) || hdr.val_len > sizeof(val_buf)) {
      printf("[!] Error: WAL record payload too large for buffers.\n");
      break;
    }

    // Reading the key from WAL
    if (fread(key_buf, 1, hdr.key_len, f) != hdr.key_len)
      break;

    // Reading the value from WAL
    if (hdr.val_len > 0) {
      if (fread(val_buf, 1, hdr.val_len, f) != hdr.val_len)
        break;
    }

    // Replaying the operation
    switch (hdr.op_type) {
    case WAL_OP_INSERT:
      db_insert_internal(db_filename, fsm, idx, key_buf,
                         hdr.val_len > 0 ? val_buf : NULL);
      break;
    case WAL_OP_UPDATE:
      db_update_internal(db_filename, fsm, idx, key_buf,
                         hdr.val_len > 0 ? val_buf : NULL);
      break;
    case WAL_OP_DELETE:
      db_delete_internal(db_filename, fsm, idx, key_buf);
      break;
    default:
      printf("[!] Unknown operation type in WAL: %d\n", hdr.op_type);
      break;
    }
  }

  fclose(f);
  printf("[+] WAL recovery completed successfully!\n");
  return 0;
}