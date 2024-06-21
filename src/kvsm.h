#ifndef __FINWO_KVSM_H__
#define __FINWO_KVSM_H__

#include <stdint.h>

#include "finwo/palloc.h"
#include "tidwall/buf.h"

struct kvsm {
  PALLOC_FD     fd;
  PALLOC_OFFSET root_offset;
  uint64_t      root_increment;
};

struct kvsm_transaction_entry {
  struct buf key;
  struct buf value;
};

struct kvsm_transaction {
  struct kvsm   *ctx;
  uint8_t        version;
  PALLOC_OFFSET  offset;
  PALLOC_OFFSET  parent;
  uint64_t       increment;
  uint16_t       flags; // 1 = hydrated
  uint16_t       header_size;
  uint16_t       entry_count;

  struct kvsm_transaction_entry *entry;
};

struct kvsm * kvsm_open(const char *filename, const int isBlockDev);
void          kvsm_close(struct kvsm *ctx);
void          kvsm_compact(struct kvsm *ctx, uint64_t increment);

struct kvsm_transaction * kvsm_transaction_init(struct kvsm *ctx);
struct kvsm_transaction * kvsm_transaction_load(struct kvsm *ctx, PALLOC_OFFSET offset);
struct buf              * kvsm_transaction_get(struct kvsm_transaction *tx, const struct buf *key);
void                      kvsm_transaction_set(struct kvsm_transaction *tx, const struct buf *key, const struct buf *value);
void                      kvsm_transaction_del(struct kvsm_transaction *tx, const struct buf *key);
void                      kvsm_transaction_commit(struct kvsm_transaction *tx);
void                      kvsm_transaction_free(struct kvsm_transaction *tx);
void                      kvsm_transaction_copy_records(struct kvsm_transaction *dst, struct kvsm_transaction *src);

// #ifndef __CHUNKMODULE_DOMAIN_TRANSACTION_H__
// #define __CHUNKMODULE_DOMAIN_TRANSACTION_H__
//
// #include <stdint.h>
//
// #include "finwo/palloc.h"
// #include "tidwall/buf.h"
//
// #include "common.h"


// struct kvsm_transaction_t {
//   PALLOC_OFFSET                    offset;
//   PALLOC_OFFSET                    parent;
//   uint64_t                         increment;
//   uint64_t                         timestamp;
//   uint16_t                         header_length;
//   unsigned int                     entry_count;
//   struct kvsm_transaction_entry_t *entry;
// };
//
// struct kvsm_transaction_t * kvsm_transaction_init();
// struct buf *                kvsm_transaction_get(struct kvsm_transaction_t *, const struct buf *);
// void                        kvsm_transaction_set(struct kvsm_transaction_t *, const struct buf *, const struct buf *);
// void                        kvsm_transaction_del(struct kvsm_transaction_t *, const struct buf *);
// void                        kvsm_transaction_copy_records(struct kvsm_transaction_t *, struct kvsm_transaction_t *);
// void                        kvsm_transaction_free(struct kvsm_transaction_t *);
//
// // Caution: does NOT load entries
// struct kvsm_transaction_t * kvsm_transaction_load(PALLOC_OFFSET);
//
// void kvsm_transaction_store(struct kvsm_transaction_t *);
//
// #endif // __CHUNKMODULE_DOMAIN_TRANSACTION_H__

#endif // __FINWO_KVSM_H__
