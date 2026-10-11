#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096

#define MAX_KEY_LEN 64
#define MAX_INDEX_ENTRIES 1000

#define MAX_PAGES 1024

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

// Metadata prepended to chunks of oversized records
typedef struct {
  int32_t next_page_id;
  int32_t next_slot_id;
  uint16_t chunk_len;
} RecordFragmentHeader;

// Free Space Map
typedef struct {
  uint16_t free_bytes[MAX_PAGES];
  int last_page_id;
} FreeSpaceMap;

// WAL operation types
typedef enum { WAL_OP_INSERT = 1, WAL_OP_DELETE, WAL_OP_UPDATE } WALOpType;

// WAL record header
typedef struct {
  uint32_t magic;
  uint8_t op_type;
  uint16_t key_len;
  uint16_t val_len;
} WALRecordHeader;

// Function prototypes
void init_page(DiskPage *page);
int insert_record_into_page(DiskPage *page, const uint8_t *record_data,
                            uint16_t length);

int write_page_to_disk(const char *filename, int page_id, const DiskPage *page);
int read_page_from_disk(const char *filename, int page_id, DiskPage *page);

void init_index(IndexTable *idx);
int index_insert(IndexTable *idx, const char *key, int page_id, int slot_id);
IndexEntry *index_lookup(IndexTable *idx, const char *key);

int db_insert_internal(const char *db_filename, FreeSpaceMap *fsm,
                       IndexTable *idx, const char *key, const char *value);

int db_insert_large(const char *db_filename, FreeSpaceMap *fsm, IndexTable *idx,
                    const char *key, const uint8_t *data, size_t total_len);

int db_get(const char *db_filename, IndexTable *idx, const char *key,
                 char *output_buffer, size_t max_out_len);

int db_delete_internal(const char *db_filename, FreeSpaceMap *fsm,
                       IndexTable *idx, const char *key);

void init_fsm(FreeSpaceMap *fsm);
int fsm_find_page(FreeSpaceMap *fsm, size_t space_needed);

int db_update_internal(const char *db_filename, FreeSpaceMap *fsm,
                       IndexTable *idx, const char *key, const char *new_value);

// WAL prototypes
int db_insert(const char *db_filename, FreeSpaceMap *fsm, IndexTable *idx,
              const char *key, const char *value);

int db_delete(const char *db_filename, FreeSpaceMap *fsm, IndexTable *idx,
              const char *key);

int db_update(const char *db_filename, FreeSpaceMap *fsm, IndexTable *idx,
              const char *key, const char *new_value);

int WAL_append(const char *wal_filename, WALOpType op, const char *key,
               const char *value);
int recover_from_wal(const char *wal_filename, const char *db_filename,
                     FreeSpaceMap *fsm, IndexTable *idx);

#endif