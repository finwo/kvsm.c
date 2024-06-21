#include <string.h>

#include "finwo/endian.h"
#include "finwo/io.h"
#include "rxi/log.h"
#include "tidwall/buf.h"

#include "kvsm.h"

/*struct kvsm {*/
/*  PALLOC_FD     fd;*/
/*  PALLOC_OFFSET root_offset;*/
/*  uint64_t      root_increment;*/
/*};*/

/*struct kvsm_transaction_entry {*/
/*  struct buf key;*/
/*  struct buf value;*/
/*};*/

/*struct kvsm_transaction {*/
/*  struct kvsm                   *ctx;*/
/*  PALLOC_OFFSET                  offset;*/
/*  PALLOC_OFFSET                  parent;*/
/*  uint64_t                       increment;*/
/*  uint16_t                       mode; // 0=?, 4=W, 2=R*/
/*  uint16_t                       entry_count;*/
/*  struct kvsm_transaction_entry *entry;*/
/*};*/

#define FLAG_HYDRATED     1
#define FLAG_NONRECURSIVE 2

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
  uint16_t header_size = 0;
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
  read_os(ctx->fd, &(tx->increment), sizeof(uint64_t     ));
  tx->parent    = be64toh(tx->parent);
  tx->increment = be64toh(tx->increment);
  log_trace("Read increment: %llu", tx->increment);
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

  log_debug("Initializing blob storage");
  PALLOC_RESPONSE r = palloc_init(ctx->fd, flags);
  if (r != PALLOC_OK) {
    log_error("Error during medium initialization", filename);
    goto kvsm_open_cleanup_palloc;
  }

  log_debug("Searching for kv root");
  PALLOC_OFFSET off = palloc_next(ctx->fd, 0);
  while(off) {
    tx = kvsm_transaction_load(ctx, off);
    if (!tx) {
      log_error("Could not load transaction at 0x%llx", off);
      goto kvsm_open_cleanup_palloc;
    }
    log_trace("Loaded tx: %llx", tx->offset);
    if (tx->increment > ctx->root_increment) {
      log_trace("Promoted to new root");
      ctx->root_increment = tx->increment;
      ctx->root_offset    = off;
    }
    kvsm_transaction_free(tx);
    off = palloc_next(ctx->fd, off);
  }

  log_debug("Detected root: %llx, %d", ctx->root_offset, ctx->root_increment);

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

// Loads v0 values from palloc into transaction
// Does not care about transaction version, just inserts
void _kvsm_transaction_hydrate_v0(struct kvsm_transaction *tx, PALLOC_OFFSET off) {
  if (!tx) {
    log_warn("transaction_hydrate_v0 ran on null transaction");
    return;
  }

  struct kvsm *ctx = tx->ctx;
  struct buf *current_value = calloc(1, sizeof(struct buf));
  struct buf *current_key   = calloc(1, sizeof(struct buf));

  uint8_t len8;
  uint16_t len16;
  uint64_t len64;
  int64_t n, c;

  seek_os(ctx->fd, off, SEEK_SET);
  log_trace("Reading tx values from %lld", off);
  while(1) {

    // Read current key length (or end marker)
    read_os(ctx->fd, &len8, sizeof(len8));
    if (len8 == 0) goto hydrate_v0_clear_clean;
    if (len8 & 128) {
      len16 = (len8 & 127) << 8;
      read_os(ctx->fd, &len8, sizeof(len8));
      len16 |= len8;
    } else {
      len16 = len8;
    }

    // Read key data
    n = 0;
    current_key->data = malloc(len16);
    current_key->len  = len16;
    current_key->cap  = len16;
    while(n < len16) {
      c = read_os(ctx->fd, current_key->data + n, len16 - n);
      if (c <= 0) {
        log_error("Error reading key at %lld", seek_os(ctx->fd, 0, SEEK_CUR));
        goto hydrate_v0_clear_midkey;
      }
      n += c;
    }

    // Read value
    if (read_os(ctx->fd, &len64, sizeof(len64)) != sizeof(len64)) {
        log_error("Error reading value length at %lld", seek_os(ctx->fd, 0, SEEK_CUR));
        goto hydrate_v0_clear_midkey;
    }
    current_value->len = be64toh(len64);
    current_value->cap = current_value->len;
    if (current_value->len) {
      current_value->data = malloc(current_value->len);
      while(n < current_value->len) {
        c = read_os(ctx->fd, current_value->data + n, current_value->len - n);
        if (c <= 0) {
          log_error("Error reading key at %lld", seek_os(ctx->fd, 0, SEEK_CUR));
          goto hydrate_v0_clear_midval;
        }
        n += c;
      }
    }

    // Append to in-memory data
    kvsm_transaction_set(tx, current_key, current_value);

    // Cleanup
    buf_clear(current_key);
    buf_clear(current_value);
  }

hydrate_v0_clear_midval:
  buf_clear(current_value);
hydrate_v0_clear_midkey:
  buf_clear(current_key);
hydrate_v0_clear_clean:
  free(current_value);
  free(current_key);
  return;
}

// Only loads values of a single transaction
// Does NOT iterate parents
void kvsm_transaction_hydrate(struct kvsm_transaction *tx) {
  if (!tx) {
    log_warn("transaction_hydrate ran on null transaction");
    return;
  }
  if (tx->flags & FLAG_HYDRATED) {
    return;
  }
  if (!tx->offset) {
    log_debug("Hydrate called on in-memory transaction");
    return;
  }
  switch(tx->version) {
    case 0:
      _kvsm_transaction_hydrate_v0(tx, tx->offset + tx->header_size);
      tx->flags |= FLAG_HYDRATED;
      break;
    default:
      log_error("Unsupported transaction version %d at %lld", tx->version, tx->offset);
      break;
  }
}

// Caution: Does NOT use in-memory values, only from disk
// Will always iterate parents
struct buf * kvsm_transaction_get(struct kvsm_transaction *tx, const struct buf *key) {
  if (!tx) {
    log_error("transaction_get ran on null transaction");
    return NULL;
  }
  if (!tx->offset) {
    log_debug("Get called on in-memory transaction");
    return NULL;
  }
  if (tx->version != 0) {
    log_error("Unsupported transaction version %d at %lld", tx->version, tx->offset);
    return NULL;
  }

  struct kvsm *ctx = tx->ctx;
  struct buf *current_value = calloc(1, sizeof(struct buf));
  struct buf *current_key   = calloc(1, sizeof(struct buf));

  uint8_t len8;
  uint16_t len16;
  uint64_t len64;
  int64_t n, c;
  PALLOC_OFFSET off = tx->offset;

  struct kvsm_transaction *_tx = kvsm_transaction_load(ctx, off);
  while(_tx) {
    if (_tx->version != 0) {
      log_error("Unsupported transaction version %d at %lld", _tx->version, _tx->offset);
      kvsm_transaction_free(_tx);
      free(current_value);
      free(current_key);
      return NULL;
    }

    seek_os(ctx->fd, _tx->offset + _tx->header_size, SEEK_SET);

    while(1) {

      // Read key length
      read_os(ctx->fd, &len8, sizeof(len8));
      if (len8 == 0) break;
      if (len8 & 128) {
        len16 = (len8 & 127) << 8;
        read_os(ctx->fd, &len8, sizeof(len8));
        len16 |= len8;
      } else {
        len16 = len8;
      }

      // Not length-matching = skip
      // TODO: more robust read
      if (len16 != key->len) {
        seek_os(ctx->fd, len16, SEEK_CUR); // Skip key data
        read_os(ctx->fd, &len64, sizeof(len64)); // Read value length
        len64 = be64toh(len64);
        seek_os(ctx->fd, len64, SEEK_CUR);       // Skip value data
        continue;
      }

      // Read key data
      n = 0;
      current_key->data = malloc(len16);
      current_key->len  = len16;
      current_key->cap  = len16;
      while(n < len16) {
        c = read_os(ctx->fd, current_key->data + n, len16 - n);
        if (c <= 0) {
          log_error("Error reading key at %lld", seek_os(ctx->fd, 0, SEEK_CUR));
          kvsm_transaction_free(_tx);
          buf_clear(current_key);
          free(current_key);
          free(current_value);
          return NULL;
        }
        n += c;
      }

      // Read value length (we'll need it regardless of match or not)
      // TODO: more robust read
      read_os(ctx->fd, &len64, sizeof(len64)); // Read value length
      len64 = be64toh(len64);

      // Skip if key not matching
      if (memcmp(current_key->data, key->data, key->len)) {
        seek_os(ctx->fd, len16, SEEK_CUR);
        buf_clear(current_key);
        continue;
      }

      // Read the actual data
      // TODO: more robust read
      current_value->data = malloc(len64);
      current_value->len  = len64;
      current_value->cap  = len64;
      read_os(ctx->fd, current_value->data, current_value->len);

      // Cleanup and return found data
      kvsm_transaction_free(_tx);
      buf_clear(current_key);
      free(current_key);
      return current_value;
    }

    off = _tx->parent;
    kvsm_transaction_free(_tx);
    _tx = kvsm_transaction_load(ctx, off);
  }

  // Not found
  buf_clear(current_value);
  buf_clear(current_key);
  free(current_value);
  free(current_key);
  return NULL;
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

  // Ensure the transaction has an increment
  if (!tx->increment) {
    tx->increment = ctx->root_increment + 1;
    log_trace("Generated increment for tx: %llu", tx->increment);
  }

  // Find neighbouring transaction to insert between
  struct kvsm_transaction *n_left  = kvsm_transaction_load(ctx, ctx->root_offset);
  struct kvsm_transaction *n_right = NULL;
  if (!tx->offset) {
    log_trace("transaction_commit: no offset, searching parents");
    while(n_left) {
      log_trace("checking parent... %llx(^%llx): %llu", n_left->offset, n_left->parent, n_left->increment);
      if (n_left->increment < tx->increment) {
        log_trace("Promoted to parent");
        tx->parent = n_left->offset;
        break;
      };
      // TODO: merge if n_left->increment == tx->increment
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
  txlen += sizeof(tx->increment);

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
  tx->parent    = htobe64(tx->parent);
  tx->increment = htobe64(tx->increment);

  // Write header
  seek_os(ctx->fd, tx->offset, SEEK_SET);
  write_os(ctx->fd, &(tx->version   ), sizeof(tx->version   ));
  write_os(ctx->fd, &(tx->parent    ), sizeof(tx->parent    ));
  write_os(ctx->fd, &(tx->increment ), sizeof(tx->increment ));

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
  tx->increment = be64toh(tx->increment);

  // Update global state
  if (tx->increment > ctx->root_increment) {
    ctx->root_offset = tx->offset;
    ctx->root_increment = tx->increment;
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
      log_debug("Found %*s = %*s", (int)(entry->key.len), entry->key.data, (int)(entry->value.len), entry->value.data);
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

// Copies records from persistent storage tx src into memory tx dst
void kvsm_transaction_copy_records(struct kvsm_transaction *dst, struct kvsm_transaction *src) {
/*  printf("cpy: %llx -> %llx\n", src->offset, dst->offset);*/
/*  if (!src->offset) return;*/
/*  if (!src->header_length) return;*/
/**/
/*  uint8_t len8;*/
/*  uint16_t len16;*/
/*  uint64_t len64;*/
/**/
/*  // Reserve buffers*/
/*  struct buf *current_value = calloc(1, sizeof(struct buf));*/
/*  struct buf *current_key  = calloc(1, sizeof(struct buf));*/
/**/
/*  seek_os(kvsm_state->fd, src->offset + src->header_length, SEEK_SET);*/
/*  while(1) {*/
/**/
/*    // Read current key length*/
/*    // 0 = end of list*/
/*    read_os(kvsm_state->fd, &len8, sizeof(len8));*/
/*    printf("Reading key length: %d\n", len8);*/
/*    if (len8 == 0) break;*/
/*    if (len8 & 128) {*/
/*      len16 = (len8 & 127) << 8;*/
/*      read_os(kvsm_state->fd, &len8, sizeof(len8));*/
/*      len16 |= len8;*/
/*    } else {*/
/*      len16 = len8;*/
/*    }*/
/**/
/*    // Read key data*/
/*    current_key->data = malloc(len16);*/
/*    current_key->len  = len16;*/
/*    read_os(kvsm_state->fd, current_key->data, len16);*/
/*    printf("Copying %.*s\n", len16, current_key->data);*/
/**/
/*    // Read value*/
/*    read_os(kvsm_state->fd, &len64, sizeof(len64)); // Read value length*/
/*    current_value->len  = be64toh(len64);*/
/*    current_value->data = malloc(current_value->len);*/
/*    read_os(kvsm_state->fd, current_value->data, current_value->len);*/
/**/
/*    // Insert into new transaction*/
/*    kvsm_transaction_set(dst, current_key, current_value);*/
/*    buf_clear(current_key);*/
/*    buf_clear(current_value);*/
/*  }*/
/**/
/*  // Done*/
/*  free(current_key);*/
/*  free(current_value);*/
}

void kvsm_compact(struct kvsm *ctx, uint64_t increment) {
  struct kvsm_transaction_t *tx_keep = kvsm_transaction_load(ctx, ctx->root_offset);



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
//   uint64_t                         increment;
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
