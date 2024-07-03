#ifndef __FINWO_KVSM_H__
#define __FINWO_KVSM_H__

/// # kvsm
///
/// Key-value storage machine
///
/// ## Installing
///
/// This library makes use of [dep](https://github.com/finwo/dep) to manage it's
/// dependencies and exports.
///
/// ```sh
/// dep add finwo/kvsm@main
/// ```
///
/// ## Usage
///
/// TODO
///
/// ## About
///
/// ### Why
///
/// Well, I wanted something simple that I can embed into other applications.
/// This library is designed to be simple, not to break any records.
///
/// ### How
///
/// This library makes use of [palloc](https://github.com/finwo/palloc.c) to
/// handle blob allocations on a file or block device. Each blob represents a
/// transaction.
///
/// From there, a transaction contains a parent reference, a time-based
/// identifier and a list of key-value pairs.
///
/// This in turn allows for out-of-order insertion of transactions, time-based
/// compaction of older transactions, and repairing of references if something
/// went wrong.
///
/// ### Example
///
/// This library includes [kvsmctl](util/kvsmctl.c) as an example program making
/// use of this library.
///
/// Many bugs were caught implementing and playing with it, but feel free to
/// [open an issue](https://github.com/finwo/kvsm.c/issues) when you encounter
/// something unexpected.

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
