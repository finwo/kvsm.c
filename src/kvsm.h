#ifndef __FINWO_KVSM_H__
#define __FINWO_KVSM_H__

#include <stdint.h>

#include "finwo/palloc.h"
#include "tidwall/buf.h"

#define KVSM_RESPONSE int
#define KVSM_OK       0
#define KVSM_ERROR    1

struct kvsm {
  PALLOC_FD     fd;
  PALLOC_OFFSET current_offset;
  uint64_t      current_increment;
};

struct kvsm_cursor {
  const struct kvsm *ctx;
  PALLOC_OFFSET      parent;
  PALLOC_OFFSET      offset;
  uint64_t           increment;
};

struct kvsm * kvsm_open(const char *filename, const int isBlockDev);
void          kvsm_close(struct kvsm *ctx);
void          kvsm_compact(struct kvsm *ctx);

struct buf  * kvsm_get(struct kvsm *ctx, const struct buf *key);
KVSM_RESPONSE kvsm_set(struct kvsm *ctx, const struct buf *key, const struct buf *value);
KVSM_RESPONSE kvsm_del(struct kvsm *ctx, const struct buf *key);

void kvsm_cursor_fetch(struct kvsm *ctx, uint64_t increment);
void kvsm_cursor_free(struct kvsm_cursor *cursor);
void kvsm_cursor_parent(struct kvsm_cursor *cursor);
void kvsm_cursor_serialize(struct kvsm_cursor *cursor);

#endif // __FINWO_KVSM_H__
