#ifndef STORAGE_H
#define STORAGE_H

#include <cstdint>
#include <stdint.h>

#define PAGE_SIZE 4096

typedef struct {
  uint16_t item_count;
  uint16_t free_space_pointer;
} PageHeader;

typedef struct {
  uint16_t offset;
  uint16_t length;
} Slot;

typedef struct {
  char data[PAGE_SIZE];
} DiskPage;

#endif