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
/// From there, a transaction contains one or more parent references, an
/// increment number and a list of key-value pairs.
///
/// This in turn allows for out-of-order insertion of transactions, compaction
/// of older transactions, and even having multiple nodes sync up their
/// transactions in a deterministic manner

#include <stdint.h>

#include "finwo/palloc.h"
/*#include "tidwall/buf.h"*/

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
  PALLOC_FD      fd;
  PALLOC_OFFSET *head;
  int            head_count;
};
///>
/// </details>


/// <details>
///   <summary>struct kvsm_transaction</summary>
///
///   TBD
///<C
struct kvsm_transaction {
  const struct kvsm *ctx;
  const struct buf  *id;
  PALLOC_OFFSET     *parent;
  int                parent_count;
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
///   <summary>kvsm_transaction_get_id(ctx, offset)</summary>
///
///   Gets the identifier of the transaction located at the given offset
///<C
struct buf * kvsm_transaction_get_id(const struct kvsm *ctx, PALLOC_OFFSET offset);
///>
/// </details>

/// <details>
///   <summary>kvsm_transaction_load_id(ctx, identifier)</summary>
///
///   Loads the metadata for the given transaction id and returns a transaction struct for it
///<C
struct kvsm_transaction * kvsm_transaction_load_id(const struct kvsm *ctx, const struct buf *identifier);
///>
/// </details>

/// <details>
///   <summary>kvsm_transaction_free(tx)</summary>
///
///   Frees up the memory used by the transaction
///<C
KVSM_RESPONSE kvsm_transaction_free(struct kvsm_transaction *tx);
///>
/// </details>

/// <details>
///   <summary>kvsm_transaction_serialize</summary>
///
///   Serializes the transaction, including contents
///<C
struct buf * kvsm_transaction_serialize(const struct kvsm_transaction *tx);
///>
/// </details>

/// <details>
///   <summary>kvsm_transaction_ingest</summary>
///
///   Stores the given transaction and it's data
///<C
KVSM_RESPONSE kvsm_transaction_ingest(const struct buf *data);
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
