#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

#define PAGE_SIZE 4096

// Header at the beginning of the page
typedef struct {
  uint16_t item_count;
  uint16_t free_space_pointer;
} PageHeader;

//  Slots pointing at records
typedef struct {
  uint16_t offset;
  uint16_t length;
} Slot;

// Raw representation of a disk page (4KB)
typedef struct {
  char data[PAGE_SIZE];
} DiskPage;

#endif