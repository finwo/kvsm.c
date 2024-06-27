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
void          kvsm_compact(const struct kvsm *ctx);

struct buf  * kvsm_get(const struct kvsm *ctx, const struct buf *key);
KVSM_RESPONSE kvsm_set(      struct kvsm *ctx, const struct buf *key, const struct buf *value);
KVSM_RESPONSE kvsm_del(      struct kvsm *ctx, const struct buf *key);

void                 kvsm_cursor_free(struct kvsm_cursor *cursor);
struct kvsm_cursor * kvsm_cursor_previous(const struct kvsm_cursor *cursor);
struct kvsm_cursor * kvsm_cursor_next(const struct kvsm_cursor *cursor);

struct kvsm_cursor * kvsm_cursor_load(const struct kvsm *ctx, PALLOC_OFFSET offset);
struct kvsm_cursor * kvsm_cursor_fetch(const struct kvsm *ctx, const uint64_t increment);

struct buf * kvsm_cursor_serialize(const struct kvsm_cursor *cursor);
KVSM_RESPONSE kvsm_cursor_ingest(struct kvsm *ctx, const struct buf *serialized);

#endif // __FINWO_KVSM_H__
