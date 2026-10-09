#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

#define PAGE_SIZE 4096

#define MAX_KEY_LEN 64
#define MAX_INDEX_ENTRIES 1000

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

// Mapping keys to their location on disk
typedef struct {
  char key[MAX_KEY_LEN];
  int page_id, slot_id;
} IndexEntry;

// An index table
typedef struct {
  IndexEntry entries[MAX_INDEX_ENTRIES];
  int count;
} IndexTable;

// Function prototypes
void init_page(DiskPage *page);
int insert_record_into_page(DiskPage *page, const uint8_t *record_data,
                            uint16_t length);

int write_page_to_disk(const char *filename, int page_id, const DiskPage *page);
int read_page_from_disk(const char *filename, int page_id, DiskPage *page);

void init_index(IndexTable *idx);
int index_insert(IndexTable *idx, const char *key, int page_id, int slot_id);
IndexEntry *index_lookup(IndexTable *idx, const char *key);

#endif