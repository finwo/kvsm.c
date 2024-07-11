# kvsm

Key-value storage machine

## Installing

This library makes use of [dep](https://github.com/finwo/dep) to manage it's
dependencies and exports.

```sh
dep add finwo/kvsm@main
```

## About

### Why

Well, I wanted something simple that I can embed into other applications.
This library is designed to be simple, not to break any records.

### How

This library makes use of [palloc](https://github.com/finwo/palloc.c) to
handle blob allocations on a file or block device. Each blob represents a
transaction.

From there, a transaction contains one or more parent references, an
increment number and a list of key-value pairs.

This in turn allows for out-of-order insertion of transactions, compaction
of older transactions, and even having multiple nodes sync up their
transactions in a deterministic manner

## API


### Definitions

<details>
  <summary>KVSM_RESPONSE</summary>

  A type declaring state-based responses

```C
#define KVSM_RESPONSE int
```

</details>
<details>
  <summary>KVSM_OK</summary>

  A response declaring the method executed succesfully

```C
#define KVSM_OK 0
```

</details>
<details>
  <summary>KVSM_ERRROR</summary>

  A response declaring the method executed with a failure

```C
#define KVSM_ERROR 1
```

</details>

### Structures

<details>
  <summary>struct kvsm</summary>

  Represents a state descriptor for kvsm, holds internal state

```C
struct kvsm {
 PALLOC_FD      fd;
 PALLOC_OFFSET *head;
 int            head_count;
};
```

</details>
<details>
  <summary>struct kvsm_transaction</summary>

  TBD

```C
struct kvsm_transaction {
 const struct kvsm *ctx;
 const struct buf  *id;
 PALLOC_OFFSET     *parent;
 int                parent_count;
};
```

</details>

### Methods

<details>
  <summary>kvsm_open(filename, isBlockDev)</summary>

  Initializes a new `struct kvsm`, handling creating the file if needed.
  Returns a new descriptor or `NULL` on failure.

```C
struct kvsm * kvsm_open(const char *filename, const int isBlockDev);
```

</details>
<details>
  <summary>kvsm_close(ctx)</summary>

  Closes the given kvsm descriptor and frees it.

```C
KVSM_RESPONSE kvsm_close(struct kvsm *ctx);
```

</details>
<details>
  <summary>kvsm_compact(ctx)</summary>

  Reduces used storage by removing all transactions only containing
  non-current versions.

```C
KVSM_RESPONSE kvsm_compact(const struct kvsm *ctx);
```

</details>
<details>
  <summary>kvsm_get(ctx, key)</summary>

  Searches the kvsm medium, returning a buffer with the value or NULL if not
  found

```C
struct buf * kvsm_get(const struct kvsm *ctx, const struct buf *key);
```

</details>
<details>
  <summary>kvsm_set(ctx, key, value)</summary>

  Writes a value to the kvsm medium on the given key

```C
KVSM_RESPONSE kvsm_set(struct kvsm *ctx, const struct buf *key, const struct buf *value);
```

</details>
<details>
  <summary>kvsm_del(ctx, key)</summary>

  Writes a tombstone to the kvsm medium on the given key

```C
#define kvsm_del(ctx,key) (kvsm_set(ctx,key,&((struct buf){ .len = 0, .cap = 0 })))
```

</details>

## Example

This library includes [kvsmctl](util/kvsmctl.c) as an example program making
use of this library.

Many bugs were caught implementing and playing with it, but feel free to
[open an issue](https://github.com/finwo/kvsm.c/issues) when you encounter
something unexpected.

