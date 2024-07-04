#include <stdint.h>
#include <string.h>

#include "finwo/endian.h"
#include "finwo/io.h"
#include "rxi/log.h"
#include "tidwall/buf.h"

#include "kvsm.h"

#define FLAG_HYDRATED     1
#define FLAG_NONRECURSIVE 2

// Loads JUST the info, not the data
struct kvsm_cursor * kvsm_cursor_load(const struct kvsm *ctx, PALLOC_OFFSET offset) {
  struct kvsm_cursor *cursor = NULL;

  // Version check
  uint8_t version;
  seek_os(ctx->fd, offset  , SEEK_SET);
  read_os(ctx->fd, &version, sizeof(version));
  if (version != 0) {
    log_error("Incompatible version at %lld", offset);
    return NULL;
  }

  // Actually reserve memory
  cursor         = calloc(1, sizeof(struct kvsm_cursor));
  cursor->offset = offset;
  cursor->ctx    = ctx;

  // Parent pointer overlaps with version, 56 bits of offset should be plenty
  // Why offset and not increment: offset is single-volume, increment may be shared between systems
  PALLOC_OFFSET parent;
  seek_os(ctx->fd, offset , SEEK_SET);
  read_os(ctx->fd, &parent, sizeof(parent));
  cursor->parent = be64toh(parent);
  // No need to remove version, we're version 0

  uint64_t increment;
  read_os(ctx->fd, &increment, sizeof(increment));
  cursor->increment = be64toh(increment);
  log_trace("Read increment %lld", cursor->increment);

  return cursor;
}

// Assumes the parent still exists and hasn't been deleted by compaction
struct kvsm_cursor * kvsm_cursor_previous(const struct kvsm_cursor *cursor) {
  if (!cursor) return NULL;
  if (!cursor->ctx) return NULL;
  if (!cursor->parent) return NULL;
  // Turns the cursor into it's own parent
  return kvsm_cursor_load(cursor->ctx, cursor->parent);
}

// Assumes the cursor still exists and hasn't been deleted by compaction
struct kvsm_cursor * kvsm_cursor_next(const struct kvsm_cursor *cursor) {
  if (!cursor) return NULL;
  if (!cursor->ctx) return NULL;
  const struct kvsm *ctx = cursor->ctx;

  PALLOC_OFFSET parent;
  PALLOC_OFFSET current = ctx->current_offset;
  PALLOC_OFFSET child   = 0;
  uint64_t increment;
  uint8_t len8;

  while(current) {
    // Sanity-check version
    seek_os(ctx->fd, current, SEEK_SET);
    read_os(ctx->fd, &len8, sizeof(len8));
    if (len8 != 0) return NULL;
    seek_os(ctx->fd, current, SEEK_SET);
    read_os(ctx->fd, &parent, sizeof(parent));
    parent = be64toh(parent);
    // No need to un-mix version, we're 0

    // Return child (a.k.a. next in time) when we find ourselves
    read_os(ctx->fd, &increment, sizeof(increment));
    increment = be64toh(increment);
    if (increment == cursor->increment) {
      return kvsm_cursor_load(ctx, child);
    }

    // Go to the parent
    child   = current;
    current = parent;
  }

  // Not found
  return NULL;
}

// Fetches a specific increment, not a specific offset
struct kvsm_cursor * kvsm_cursor_fetch(const struct kvsm *ctx, const uint64_t increment) {
  if (!ctx) return NULL;

  PALLOC_OFFSET parent;
  PALLOC_OFFSET current = ctx->current_offset;
  PALLOC_OFFSET child   = ctx->current_offset;
  uint64_t _increment;
  uint8_t len8;

  while(current) {
    // Sanity-check version
    seek_os(ctx->fd, current, SEEK_SET);
    read_os(ctx->fd, &len8, sizeof(len8));
    if (len8 != 0) return NULL;
    seek_os(ctx->fd, current, SEEK_SET);
    read_os(ctx->fd, &parent, sizeof(parent));
    parent = be64toh(parent);
    // No need to un-mix version, we're 0
    read_os(ctx->fd, &_increment, sizeof(_increment));
    _increment = be64toh(_increment);

    // Found target increment
    if (_increment == increment) {
      return kvsm_cursor_load(ctx, current);
    }

    // Found target increment
    if (_increment <= increment) {
      return kvsm_cursor_load(ctx, child);
    }

    // TODO: make optional
    // Return if we're the "oldest"
    if (!parent) {
      return kvsm_cursor_load(ctx, current);
    }

    // Go to the parent
    child   = current;
    current = parent;
  }

  // Not found
  return NULL;
}

struct kvsm * kvsm_open(const char *filename, const int isBlockDev) {
  log_trace("call: kvsm_open(%s,%d)", filename, isBlockDev);
  struct kvsm_cursor *cursor = NULL;
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
    return NULL;
  }

  log_debug("Initializing blob storage");
  PALLOC_RESPONSE r = palloc_init(ctx->fd, flags);
  if (r != PALLOC_OK) {
    log_error("Error during medium initialization", filename);
    palloc_close(ctx->fd);
    return NULL;
  }

  log_debug("Searching for kv root");
  PALLOC_OFFSET off = palloc_next(ctx->fd, 0);
  while(off) {
    log_trace("Scanning %lld for root", off);
    cursor = kvsm_cursor_load(ctx, off);
    if (!cursor) {
      log_trace("Not supported: %lld", off);
      off = palloc_next(ctx->fd, off);
      continue;
    }

    log_trace("Loaded tx: %llx, %lld", cursor->offset, cursor->increment);
    if (cursor->increment > ctx->current_increment) {
      log_trace("Promoted to new root");
      ctx->current_increment = cursor->increment;
      ctx->current_offset    = off;
    }

    kvsm_cursor_free(cursor);
    off = palloc_next(ctx->fd, off);
  }

  log_debug("Detected root: %llx, %d", ctx->current_offset, ctx->current_increment);
  return ctx;
}

KVSM_RESPONSE kvsm_close(struct kvsm *ctx) {
  if (!ctx) return KVSM_ERROR;
  palloc_close(ctx->fd);
  free(ctx);
  return KVSM_OK;
}

KVSM_RESPONSE kvsm_cursor_free(struct kvsm_cursor *cursor) {
  free(cursor);
  return KVSM_OK;
}

struct _kvsm_get_response {
  const struct buf *key;
  struct buf *value;
  uint64_t increment;
};
// DOES support multi-value transactions
struct _kvsm_get_response * _kvsm_get(const struct kvsm *ctx, const struct buf *key, bool load_value) {
  log_trace("call: kvsm_get(...)");

  if (key->len >= 32768) {
    log_error("key too large");
    return NULL;
  }

  uint8_t len8;
  uint16_t len16;
  uint64_t len64;
  PALLOC_OFFSET off = ctx->current_offset;
  PALLOC_OFFSET parent;
  uint64_t increment;
  struct buf k = {};
  struct buf *v = NULL;
  struct _kvsm_get_response *resp = NULL;

  while(off) {
    log_trace("Checking %lld", off);
    seek_os(ctx->fd, off, SEEK_SET);
    read_os(ctx->fd, &len8, sizeof(len8));
    if (len8 != 0) return NULL;

    seek_os(ctx->fd, off, SEEK_SET);
    read_os(ctx->fd, &parent, sizeof(parent));
    parent = be64toh(parent);

    read_os(ctx->fd, &increment, sizeof(increment));
    increment = be64toh(increment);

    while(true) {

      // Read key length
      read_os(ctx->fd, &len8, sizeof(len8));
      if (!len8) break;
      len16 = len8 & 127;
      if (len8 & 128) {
        len16 = len16 << 8;
        read_os(ctx->fd, &len8, sizeof(len8));
        len16 |= len8;
      }

      // Read key data
      k.data = malloc(len16);
      k.len  = len16;
      k.cap  = len16;
      read_os(ctx->fd, k.data, k.len);

      // Read value length
      read_os(ctx->fd, &len64, sizeof(len64));
      len64 = be64toh(len64);

      // Different length = no match
      if (k.len != key->len) {
        seek_os(ctx->fd, len64, SEEK_CUR);
        buf_clear(&k);
        continue;
      }

      // Different data = no match
      if (memcmp(k.data, key->data, k.len)) {
        seek_os(ctx->fd, len64, SEEK_CUR);
        buf_clear(&k);
        continue;
      }

      // Here = found
      buf_clear(&k);

      // Handle delete marker response
      if (!len64) {
        return NULL;
      }

      resp = malloc(sizeof(struct _kvsm_get_response));
      if (!resp) {
        log_error("Error during memory allocation for get return wrapper");
        return NULL;
      }
      resp->increment = increment;
      resp->key       = key;

      if (load_value) {
        v = calloc(1, sizeof(struct buf));
        if (!v) {
          log_error("Error during memory allocation for get return struct");
          return NULL;
        }
        v->len = len64;
        v->cap = len64;
        v->data = malloc(len64);
        if (!v->data) {
          free(v);
          log_error("Error during memory allocation for get return blob");
          return NULL;
        }

        read_os(ctx->fd, v->data, len64);
        resp->value = v;
      }

      return resp;
    }

    off = parent;
  }

  // Not found
  return NULL;
}
struct buf  * kvsm_get(const struct kvsm *ctx, const struct buf *key) {
  struct _kvsm_get_response *response = _kvsm_get(ctx, key, true);
  if (!response) return NULL;
  struct buf *value = response->value;
  free(response);
  return value;
}

// Only gets the increment
uint64_t kvsm_get_increment(const struct kvsm *ctx, const struct buf *key) {
  struct _kvsm_get_response *response = _kvsm_get(ctx, key, false);
  if (!response) return 0;
  uint64_t increment = response->increment;
  free(response);
  return increment;
}

// DOES NOT support multi-value transactions
// Does close the list as if it supports them though
KVSM_RESPONSE kvsm_set(struct kvsm *ctx, const struct buf *key, const struct buf *value) {
  log_trace("call: kvsm_set(...)");

  if (key->len >= 32768) {
    log_error("key too large");
    return KVSM_ERROR;
  }

  // Calculate transaction size
  size_t tx_size = 0;
  tx_size += 8; // version|parent
  tx_size += 8; // increment
  if (key->len >= 128) {
    tx_size += 2;
  } else {
    tx_size += 1;
  }
  tx_size += key->len;
  tx_size += 8; // value size
  tx_size += value->len;
  tx_size += 1; // End-of-list

  log_trace("Reserving %lld bytes", tx_size);
  PALLOC_OFFSET offset = palloc(ctx->fd, tx_size);
  PALLOC_OFFSET parent = htobe64(ctx->current_offset);
  // No need to merge version, we're 0
  seek_os(ctx->fd, offset, SEEK_SET);
  write_os(ctx->fd, &parent, sizeof(parent));
  parent = be64toh(parent);

  uint64_t increment = htobe64(ctx->current_increment + 1);
  write_os(ctx->fd, &increment, sizeof(parent));
  increment = be64toh(increment);

  uint8_t len8;
  if (key->len >= 128) {
    len8 = 128 | (key->len >> 8);
    write_os(ctx->fd, &len8, sizeof(len8));
    len8 = key->len & 255;
    write_os(ctx->fd, &len8, sizeof(len8));
  } else {
    len8 = key->len;
    write_os(ctx->fd, &len8, sizeof(len8));
  }

  write_os(ctx->fd, key->data, key->len);

  uint64_t valsize = htobe64(value->len);
  write_os(ctx->fd, &valsize, sizeof(valsize));
  write_os(ctx->fd, value->data, value->len);

  len8 = 0;
  write_os(ctx->fd, &len8, sizeof(len8));

  if (increment > ctx->current_increment) {
    ctx->current_increment = increment;
    ctx->current_offset    = offset;
  }

  return KVSM_OK;
}

// Caution: lazy algorithm
// Goes through every "transaction", and discards them if it only contains non-current versions
struct kvsm_compact_track {
  uint16_t  len;
  char     *dat;
};
KVSM_RESPONSE kvsm_compact(const struct kvsm *ctx) {
  PALLOC_OFFSET child = 0;
  PALLOC_OFFSET current = ctx->current_offset;
  PALLOC_OFFSET parent = 0;
  PALLOC_OFFSET tmp    = 0;

  bool discardable;
  uint64_t current_increment;

  uint8_t len8;
  uint16_t len16;
  uint64_t len64;

  struct buf key;

  while(current) {
    discardable = true;
    log_trace("Checking 0x%llx for being discardable", current);

    seek_os(ctx->fd, current, SEEK_SET);
    read_os(ctx->fd, &len8, sizeof(len8));
    if (len8 != 0) return KVSM_ERROR; // Only version 0 supported

    seek_os(ctx->fd, current, SEEK_SET);
    read_os(ctx->fd, &parent, sizeof(parent));
    parent = be64toh(parent);
    // No need to un-mix version, we're 0

    read_os(ctx->fd, &current_increment, sizeof(current_increment));
    current_increment = be64toh(current_increment);

    while(true) {
      log_trace("Cursor: 0x%llx", seek_os(ctx->fd, 0, SEEK_CUR));

      // Read key length
      read_os(ctx->fd, &len8, sizeof(len8));
      log_trace("Len: %d", len8);
      if (!len8) break; // End of list
      len16 = len8 & 127;
      if (len8 >= 128) {
        len16 = len16 << 8;
        read_os(ctx->fd, &len8, sizeof(len8));
        len16 |= len8;
      }
      log_trace("Keylen %d in 0x%llx", len16, current);

      // Read key data
      key.data = malloc(len16);
      key.len  = len16;
      key.cap  = len16;
      read_os(ctx->fd, key.data, key.len);

      // Read value length
      read_os(ctx->fd, &len64, sizeof(len64));
      len64 = be64toh(len64);

      // Save current location, fetching increment moves fd cursor
      tmp = seek_os(ctx->fd, 0, SEEK_CUR);

      // Mark as non-discardable if it's being used for the found key
      if (kvsm_get_increment(ctx, &key) == current_increment) {
        discardable = false;
        seek_os(ctx->fd, tmp, SEEK_SET);
        buf_clear(&key);
        break;
      }

      // Return to after data to continue checking the list
      buf_clear(&key);
      seek_os(ctx->fd, tmp+len64, SEEK_SET);

    }

    log_trace("status for %llx: %sdiscardable", current, discardable ? "" : "not ");

    // Go to the next transaction
    if (!discardable) {
      child   = current;
      current = parent;
      continue;
    }

    log_debug("Discarding increment %d at %llx", current_increment, current);

    if (child) {
      // Update child to point to our parent
      // Sanity check
      seek_os(ctx->fd, child, SEEK_SET);
      read_os(ctx->fd, &len8, sizeof(len8));
      if (len8 != 0) return KVSM_ERROR; // Only version 0 supported, bail

      seek_os(ctx->fd, child, SEEK_SET);
      // No need to mix version, we're 0
      parent = htobe64(parent);
      write_os(ctx->fd, &parent, sizeof(parent));
      parent = be64toh(parent);
    } else {
      // No child, but we won't remove root, it's always up-to-date
    }

    // Free used space
    pfree(ctx->fd, current);
    current = parent;
    // Don't update child, hasn't changed
  }

  return KVSM_OK;
}

struct buf * kvsm_cursor_serialize(const struct kvsm_cursor *cursor) {
  log_trace("call: kvsm_cursor_serialize(%lld)", cursor->increment);
  const struct kvsm *ctx = cursor->ctx;

  uint8_t len8;
  uint16_t len16;
  uint64_t len64;

  // Sanity-check the version
  seek_os(ctx->fd, cursor->offset, SEEK_SET);
  read_os(ctx->fd, &len8, sizeof(len8));
  if (len8 != 0) return NULL;

  struct buf *output = calloc(1, sizeof(struct buf));

  buf_append_byte(output, 0); // Serialized format 0

  seek_os(ctx->fd, cursor->offset + sizeof(PALLOC_OFFSET), SEEK_SET);
  read_os(ctx->fd, &len64, sizeof(len64));
  buf_append(output, &len64, sizeof(len64));

  while(1) {
    read_os(ctx->fd, &len8, sizeof(len8));
    buf_append(output, &len8, sizeof(len8));
    if (!len8) break;

    len16 = len8 & 127;
    if (len8 & 128) {
      len16 = len16 << 8;
      read_os(ctx->fd, &len8, sizeof(len8));
      buf_append(output, &len8, sizeof(len8));
      len16 |= len8;
    }

    if ((output->cap - output->len) < len16) {
      output->data = realloc(output->data, output->len + len16);
      output->cap  = output->len + len16;
    }
    read_os(ctx->fd, output->data + output->len, len16);
    output->len += len16;

    read_os(ctx->fd, &len64, sizeof(len64));
    buf_append(output, &len64, sizeof(len64));
    len64 = be64toh(len64);
    if ((output->cap - output->len) < len64) {
      output->data = realloc(output->data, output->len + len64);
      output->cap  = output->len + len64;
    }
    read_os(ctx->fd, output->data + output->len, len64);
    output->len += len64;
  }

  return output;
}

KVSM_RESPONSE kvsm_cursor_ingest(struct kvsm *ctx, const struct buf *serialized) {
  log_trace("call: kvsm_cursor_ingest(...)");

  if (serialized->len < 1) {
    log_error("Invalid length to ingest");
    return KVSM_ERROR;
  }

  if (serialized->data[0] != 0) {
    log_error("Ingestable has unsupported version");
    return KVSM_ERROR;
  }

  uint8_t len8;
  uint8_t len64;
  uint64_t increment;
  memcpy(&increment, &(serialized->data[1]), sizeof(increment));
  increment = be64toh(increment);

  PALLOC_OFFSET child   = 0;
  PALLOC_OFFSET current = 0;
  PALLOC_OFFSET parent  = 0;
  if (increment > ctx->current_increment) {
    // Newer than we currently have, dangle at the front
    parent = ctx->current_offset;
  } else {

    current = ctx->current_offset;
    while(1) {
      seek_os(ctx->fd, current, SEEK_SET);
      read_os(ctx->fd, &len8, sizeof(len8));
      if (len8 != 0) {
        log_error("Encountered unsupported transaction version %d at %lld", len8, seek_os(ctx->fd, 0, SEEK_CUR) - 1);
        return KVSM_ERROR;
      }

      seek_os(ctx->fd, current, SEEK_SET);
      read_os(ctx->fd, &parent, sizeof(parent));
      parent = be64toh(parent);
      log_trace("Parent of %llx is %llx", current, parent);
      read_os(ctx->fd, &len64, sizeof(len64));
      len64 = be64toh(len64);

      // No more parents
      // Return to dangle at the end
      if (!parent) {
        child = current;
        break;
      }

      // Found spot to insert between
      if (len64 < increment) {
        parent = current;
        break;
      }

      // Loop to the next parent
      child   = current;
      current = parent;
    }
  }

  // Start actually writing
  current = palloc(ctx->fd, serialized->len + sizeof(parent) - 1);
  seek_os(ctx->fd, current, SEEK_SET);
  // No need to mix version, we're 0
  log_debug("Ingesting with parent %llx", parent);
  parent = htobe64(parent);
  write_os(ctx->fd, &parent, sizeof(parent));
  // Write serialized data as-is, it matches our on-disk format
  write_os(ctx->fd, serialized->data + 1, serialized->len - 1);

  // Update child's parent pointer if we got one
  log_debug("Ingesting with child %llx", child);
  if (child) {
    current = htobe64(current);
    seek_os(ctx->fd, child, SEEK_SET);
    write_os(ctx->fd, &current, sizeof(current));
  }

  // Update root marker if needed
  if (increment > ctx->current_increment) {
    ctx->current_increment = increment;
    ctx->current_offset    = current;
  }

  return KVSM_OK;
}
