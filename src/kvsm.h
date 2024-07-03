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
/// From there, a transaction contains a parent reference, an increment number
/// and a list of key-value pairs.
///
/// This in turn allows for out-of-order insertion of transactions, compaction
/// of older transactions, and repairing of references if something went wrong.

#include <stdint.h>

#include "finwo/palloc.h"
#include "tidwall/buf.h"

///
/// ## API
///

///
/// ### Definitions
///

/// <details>
///   <summary>KVSM_RESPONSE</summary>
///
///   A type declaring state-based responses
///<C
#define KVSM_RESPONSE int
///>
/// </details>

/// <details>
///   <summary>KVSM_OK</summary>
///
///   A response declaring the method executed succesfully
///<C
#define KVSM_OK 0
///>
/// </details>

/// <details>
///   <summary>KVSM_ERRROR</summary>
///
///   A response declaring the method executed with a failure
///<C
#define KVSM_ERROR 1
///>
/// </details>

///
/// ### Structures
///

/// <details>
///   <summary>struct kvsm</summary>
///
///   Represents a state descriptor for kvsm, holds internal state
///<C
struct kvsm {
  PALLOC_FD     fd;
  PALLOC_OFFSET current_offset;
  uint64_t      current_increment;
};
///>
/// </details>

/// <details>
///   <summary>struct kvsm_cursor</summary>
///
///   Represents a cursor to a kvsm increment/transaction
///<C
struct kvsm_cursor {
  const struct kvsm *ctx;
  PALLOC_OFFSET      parent;
  PALLOC_OFFSET      offset;
  uint64_t           increment;
};
///>
/// </details>

///
/// ### Methods
///

/// <details>
///   <summary>kvsm_open(filename, isBlockDev)</summary>
///
///   Initializes a new `struct kvsm`, handling creating the file if needed.
///   Returns a new descriptor or `NULL` on failure.
///<C
struct kvsm * kvsm_open(const char *filename, const int isBlockDev);
///>
/// </details>

/// <details>
///   <summary>kvsm_close(ctx)</summary>
///
///   Closes the given kvsm descriptor and frees it.
///<C
KVSM_RESPONSE kvsm_close(struct kvsm *ctx);
///>
/// </details>

/// <details>
///   <summary>kvsm_compact(ctx)</summary>
///
///   Reduces used storage by removing all transactions only containing
///   non-current versions.
///<C
KVSM_RESPONSE kvsm_compact(const struct kvsm *ctx);
///>
/// </details>

/// <details>
///   <summary>kvsm_get(ctx, key)</summary>
///
///   Searches the kvsm medium, returning a buffer with the value or NULL if not
///   found
///<C
struct buf * kvsm_get(const struct kvsm *ctx, const struct buf *key);
///>
/// </details>

/// <details>
///   <summary>kvsm_set(ctx, key, value)</summary>
///
///   Writes a value to the kvsm medium on the given key
///<C
KVSM_RESPONSE kvsm_set(struct kvsm *ctx, const struct buf *key, const struct buf *value);
///>
/// </details>

/// <details>
///   <summary>kvsm_del(ctx, key)</summary>
///
///   Writes a tombstone to the kvsm medium on the given key
///<C
#define kvsm_del(ctx,key) (kvsm_set(ctx,key,&((struct buf){ .len = 0, .cap = 0 })))
///>
/// </details>

/// <details>
///   <summary>kvsm_cursor_free(cursor)</summary>
///
///   Frees the used memory by the given cursor
///<C
KVSM_RESPONSE kvsm_cursor_free(struct kvsm_cursor *cursor);
///>
/// </details>

/// <details>
///   <summary>kvsm_cursor_previous(cursor)</summary>
///
///   Returns a NEW cursor, pointing to the given cursor's parent transaction,
///   or NULL if the given cursor has no parent
///<C
struct kvsm_cursor * kvsm_cursor_previous(const struct kvsm_cursor *cursor);
///>
/// </details>

/// <details>
///   <summary>kvsm_cursor_next(cursor)</summary>
///
///   Returns a NEW cursor, pointing to the given cursor's child transaction,
///   or NULL if the given cursor has no child
///<C
struct kvsm_cursor * kvsm_cursor_next(const struct kvsm_cursor *cursor);
///>
/// </details>

/// <details>
///   <summary>kvsm_cursor_load(ctx, offset)</summary>
///
///   Returns a new cursor, loaded from the transaction at the given offset.
///<C
struct kvsm_cursor * kvsm_cursor_load(const struct kvsm *ctx, PALLOC_OFFSET offset);
///>
/// </details>

/// <details>
///   <summary>kvsm_cursor_fetch(ctx, increment)</summary>
///
///   Returns a new cursor pointing to the transaction with the given increment,
///   or to the oldest transaction available higher than the given increment.
///<C
struct kvsm_cursor * kvsm_cursor_fetch(const struct kvsm *ctx, const uint64_t increment);
///>
/// </details>

/// <details>
///   <summary>kvsm_cursor_serialize(cursor)</summary>
///
///   Returns a buffer representing the serialized transaction, including
///   increment and values
///<C
struct buf * kvsm_cursor_serialize(const struct kvsm_cursor *cursor);
///>
/// </details>

/// <details>
///   <summary>kvsm_cursor_ingest(ctx, serialized)</summary>
///
///   Ingests the given serialized transaction, inserting it with the existing
///   increment instead of writing a new one
///<C
KVSM_RESPONSE kvsm_cursor_ingest(struct kvsm *ctx, const struct buf *serialized);
///>
/// </details>

///
/// ## Example
///
/// This library includes [kvsmctl](util/kvsmctl.c) as an example program making
/// use of this library.
///
/// Many bugs were caught implementing and playing with it, but feel free to
/// [open an issue](https://github.com/finwo/kvsm.c/issues) when you encounter
/// something unexpected.
///

#endif // __FINWO_KVSM_H__
