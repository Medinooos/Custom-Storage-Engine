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