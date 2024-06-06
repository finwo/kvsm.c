#include <string.h>

#include "finwo/endian.h"
#include "finwo/io.h"
#include "rxi/log.h"
#include "tidwall/buf.h"

#include "kvsm.h"

#if defined(_WIN32) || defined(_WIN64)
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

/*struct kvsm {*/
/*  PALLOC_FD     fd;*/
/*  PALLOC_OFFSET root_offset;*/
/*  uint64_t      root_tstamp;*/
/*};*/

/*struct kvsm_transaction_entry {*/
/*  struct buf key;*/
/*  struct buf value;*/
/*};*/

/*struct kvsm_transaction {*/
/*  struct kvsm                   *ctx;*/
/*  PALLOC_OFFSET                  offset;*/
/*  PALLOC_OFFSET                  parent;*/
/*  uint64_t                       tstamp;*/
/*  uint16_t                       mode; // 0=?, 4=W, 2=R*/
/*  uint16_t                       entry_count;*/
/*  struct kvsm_transaction_entry *entry;*/
/*};*/

#define FLAG_HYDRATED     1
#define FLAG_NONRECURSIVE 2

uint64_t _now() {
#if defined(_WIN32) || defined(_WIN64)
  struct _timeb timebuffer;
  _ftime(&timebuffer);
  return (uint64_t)(((timebuffer.time * 1000) + timebuffer.millitm));
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * ((uint64_t)1000)) + (tv.tv_usec / 1000);
#endif
}

struct kvsm_transaction * kvsm_transaction_init(struct kvsm *ctx) {
  struct kvsm_transaction *tx = calloc(1, sizeof(*tx));
  if (!tx) {
    log_error("Unable to reserve memory for a new transaction");
    return NULL;
  }

  tx->ctx = ctx;
  return tx;
}

// Caution: does NOT load entries
struct kvsm_transaction * kvsm_transaction_load(struct kvsm *ctx, PALLOC_OFFSET offset) {
  struct kvsm_transaction *tx;
  uint8_t  version;
  uint16_t header_size = sizeof(version);
  if (!offset) return NULL;

  // Fetch the encoding version of the transaction
  seek_os(ctx->fd, offset, SEEK_SET);
  read_os(ctx->fd, &version, sizeof(version));
  if (version != 0) {
    // We only support version 0
    log_error("Unsupported transaction at 0x%llx: invalid version", offset);
    return NULL;
  }
  header_size += sizeof(version);

  tx = kvsm_transaction_init(ctx);
  if (!tx) {
    log_error("Unable to initialize bare transaction");
    return NULL;
  }

  tx->version = version;
  tx->offset  = offset;
  /*seek_os(ctx->fd, offset + header_size, SEEK_SET);*/
  read_os(ctx->fd, &(tx->parent), sizeof(PALLOC_OFFSET));
  read_os(ctx->fd, &(tx->tstamp), sizeof(uint64_t     ));
  tx->parent = be64toh(tx->parent);
  tx->tstamp = be64toh(tx->tstamp);
  log_trace("Read timestamp: %llu", tx->tstamp);
  header_size += sizeof(PALLOC_OFFSET);
  header_size += sizeof(uint64_t     );

  tx->header_size = header_size;
  return tx;
}

struct kvsm * kvsm_open(const char *filename, const int isBlockDev) {
  log_trace("call: kvsm_open(%s,%d)", filename, isBlockDev);
  struct kvsm_transaction *tx = NULL;
  PALLOC_FLAGS flags = PALLOC_DEFAULT;
  if (!isBlockDev) flags |= PALLOC_DYNAMIC;

  struct kvsm *ctx = calloc(1, sizeof(*ctx));
  if (!ctx) {
    log_error("Could not reserve memory for kvsm context");
    return NULL;
  }

  ctx->fd = palloc_open(filename, flags);
  if (!ctx->fd) {
    log_error("Could not open storage medium: %d", filename);
    goto kvsm_open_cleanup_bare;
  }

  log_info("Initializing blob storage");
  PALLOC_RESPONSE r = palloc_init(ctx->fd, flags);
  if (r != PALLOC_OK) {
    log_error("Error during medium initialization", filename);
    goto kvsm_open_cleanup_palloc;
  }

  log_info("Searching for kv root");
  PALLOC_OFFSET off = palloc_next(ctx->fd, 0);
  while(off) {
    tx = kvsm_transaction_load(ctx, off);
    if (!tx) {
      log_error("Could not load transaction at 0x%llx", off);
      goto kvsm_open_cleanup_palloc;
    }
    log_trace("Loaded tx: %llx", tx->offset);
    if (tx->tstamp > ctx->root_tstamp) {
      log_trace("Promoted to new root");
      ctx->root_tstamp = tx->tstamp;
      ctx->root_offset = off;
    }
    kvsm_transaction_free(tx);
    off = palloc_next(ctx->fd, off);
  }

  log_debug("Detected root: %llx, %d", ctx->root_offset, ctx->root_tstamp);

  return ctx;

kvsm_open_cleanup_palloc:
  palloc_close(ctx->fd);
kvsm_open_cleanup_bare:
  if (tx) kvsm_transaction_free(tx);
  free(ctx);
  return NULL;
}

void kvsm_close(struct kvsm *ctx) {
  if (!ctx) return;
  palloc_close(ctx->fd);
  free(ctx);
}

void kvsm_transaction_hydrate(struct kvsm_transaction *tx) {
  if (!tx) {
    log_warn("transaction_hydrate ran on null transaction");
    return;
  }
  if (!tx->offset) {
    log_debug("Hydrate called on in-memory transaction");
    return;
  }

}

void kvsm_transaction_get(struct kvsm_transaction *tx, const struct buf *key) {
  if (!tx) {
    log_error("transaction_get ran on null transaction");
    return;
  }
  if (tx->flags & FLAG_HYDRATED) {
    tx->flags |= FLAG_HYDRATED;
    kvsm_transaction_hydrate(tx);
  }

  log_error("transaction_get not implemented");
}

void kvsm_transaction_set(struct kvsm_transaction *tx, const struct buf *key, const struct buf *value) {
  if (!tx) {
    log_error("transaction_set ran on null transaction");
    return;
  }
  if (tx->flags & FLAG_HYDRATED) {
    tx->flags |= FLAG_HYDRATED;
    kvsm_transaction_hydrate(tx);
  }

  // Override if duplicate key in transaction
  int i;
  for( i = 0 ; i < tx->entry_count ; i++ ) {
    if (key->len != tx->entry[i].key.len) continue;
    if (memcmp(key->data, tx->entry[i].key.data, key->len)) continue;
    // Here = found
    buf_clear(&(tx->entry[i].value));
    buf_append(&(tx->entry[i].value), value->data, value->len);
    return;
  }

  // New entry in the transaction
  tx->entry = realloc(tx->entry, sizeof(struct kvsm_transaction_entry)*(tx->entry_count + 1));
  memset(&(tx->entry[tx->entry_count]), 0, sizeof(struct kvsm_transaction_entry));
  buf_append(&(tx->entry[tx->entry_count].key  ), key->data, key->len);
  buf_append(&(tx->entry[tx->entry_count].value), value->data, value->len);
  tx->entry_count++;
}

void kvsm_transaction_del(struct kvsm_transaction *tx, const struct buf *key) {
  return kvsm_transaction_set(tx, key, &((struct buf){ .len = 0, .cap = 0 }));
}

// Caution: does NOT write end-of-list indicator if entryless transaction is given
void kvsm_transaction_commit(struct kvsm_transaction *tx) {
  int i;
  uint8_t len8;
  uint16_t len16;
  uint64_t len64;
  struct kvsm *ctx;

  if (!tx) {
    log_error("transaction_commit ran on null transaction");
    return;
  }
  if (!tx->ctx) {
    log_warn("transaction_commit ran on contextless transaction");
    return;
  } else {
    ctx = tx->ctx;
  }

  // Ensure the transaction has a timestamp
  if (!tx->tstamp) {
    tx->tstamp = _now();
    log_trace("Generated tstamp for tx: %llu", tx->tstamp);
  }

  // Find neighbouring transaction to insert between
  struct kvsm_transaction *n_left  = kvsm_transaction_load(ctx, ctx->root_offset);
  struct kvsm_transaction *n_right = NULL;
  if (!tx->offset) {
    log_trace("transaction_commit: no offset, searching parents");
    while(n_left) {
      log_trace("checking parent... %llx(^%llx): %llu", n_left->offset, n_left->parent, n_left->tstamp);
      if (n_left->tstamp < tx->tstamp) {
        log_trace("Promoted to parent");
        tx->parent = n_left->offset;
        break;
      };
      // TODO: merge if n_left->tstamp == tx->tstamp
      if (n_right) kvsm_transaction_free(n_right);
      n_right = n_left;
      n_left  = kvsm_transaction_load(ctx, n_right->parent);
    }
    // Assign the correct parent
    if (n_left) {
      log_trace("  found parent: %llx", n_left->offset);
      tx->parent = n_left->offset;
    }
  }

  // Start to "calculate" serialized transaction length

  // Header
  uint64_t txlen = 0;
  txlen += sizeof(tx->version);
  txlen += sizeof(tx->parent);
  txlen += sizeof(tx->tstamp);

  // Entries
  for( i = 0 ; i < tx->entry_count ; i++ ) {
    if (tx->entry[i].key.len > 127) { txlen += 2; } else { txlen += 1; } // key length indicator
    txlen += tx->entry[i].key.len;
    txlen += sizeof(uint64_t);
    txlen += tx->entry[i].value.len;
  }
  txlen += 1; // End of record list

  // Reserve on-disk space
  if (!tx->offset) tx->offset = palloc(ctx->fd, txlen);
  if (!tx->offset) {
    log_error("Could not reserve storage space for transaction: %d", txlen);
    if (n_left ) kvsm_transaction_free(n_left );
    if (n_right) kvsm_transaction_free(n_right);
    return;
  }

  // Prep data
  //tx->version = htobe8(tx->version);
  tx->parent  = htobe64(tx->parent);
  tx->tstamp  = htobe64(tx->tstamp);

  // Write header
  seek_os(ctx->fd, tx->offset, SEEK_SET);
  write_os(ctx->fd, &(tx->version), sizeof(tx->version));
  write_os(ctx->fd, &(tx->parent ), sizeof(tx->parent ));
  write_os(ctx->fd, &(tx->tstamp ), sizeof(tx->tstamp ));

  // Write entries
  for( i = 0 ; i < tx->entry_count ; i++ ) {
    if (tx->entry[i].key.len > 32767) {
      log_warn("Encountered too-long key during transaction commit");
    } else if (tx->entry[i].key.len > 127) {
      len16 = htobe16(32768 | tx->entry[i].key.len);
      write_os(ctx->fd, &len16, sizeof(len16));
    } else {
      len8 = tx->entry[i].key.len;
      write_os(ctx->fd, &len8, sizeof(len8));
    }
    write_os(ctx->fd, tx->entry[i].key.data, tx->entry[i].key.len);
    len64 = htobe64(tx->entry[i].value.len);
    write_os(ctx->fd, &len64, sizeof(len64));
    write_os(ctx->fd, tx->entry[i].value.data, tx->entry[i].value.len);
  }
  if (!tx->entry_count) {
    len8 = 0;
    write_os(ctx->fd, &len8, sizeof(len8));
  }

  // Revert data
  tx->parent = be64toh(tx->parent);
  tx->tstamp = be64toh(tx->tstamp);

  // Update global state
  if (tx->tstamp > ctx->root_tstamp) {
    ctx->root_offset = tx->offset;
    ctx->root_tstamp = tx->tstamp;
  }

  // Update our right neighbour
  if (n_right) {
    if (n_right->parent != tx->offset) {
      n_right->parent = tx->offset;
      kvsm_transaction_commit(n_right);
    }

    // TODO: remove now unreachable transaction
  }

  if (n_left ) kvsm_transaction_free(n_left );
  if (n_right) kvsm_transaction_free(n_right);
}

void kvsm_transaction_free(struct kvsm_transaction *tx) {
  unsigned int i;
  struct kvsm_transaction_entry *entry;

  // Assumes entry_count and entry are properly tracked
  if (tx->entry_count) {

    for( i = 0 ; i < tx->entry_count ; i++ ) {
      entry = &(tx->entry[i]);
      printf("Found %*s = %*s\n", (int)(entry->key.len), entry->key.data, (int)(entry->value.len), entry->value.data);
      buf_clear(&(entry->key))  ;
      buf_clear(&(entry->value));
      // DO NOT free entry itself
    }
  }
  if (tx->entry) {
    free(tx->entry);
  }

  free(tx);
}











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
