# kvsm

Key-value storage machine

## Installing

This library makes use of [dep](https://github.com/finwo/dep) to manage it's
dependencies and exports.

```sh
dep add finwo/kvsm@main
```

## Usage

TODO

## About

### Why

Well, I wanted something simple that I can embed into other applications.
This library is designed to be simple, not to break any records.

### How

This library makes use of [palloc](https://github.com/finwo/palloc.c) to
handle blob allocations on a file or block device. Each blob represents a
transaction.

From there, a transaction contains a parent reference, a time-based
identifier and a list of key-value pairs.

This in turn allows for out-of-order insertion of transactions, time-based
compaction of older transactions, and repairing of references if something
went wrong.

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
 PALLOC_FD     fd;
 PALLOC_OFFSET current_offset;
 uint64_t      current_increment;
};
```

</details>
<details>
  <summary>struct kvsm_cursor</summary>

  Represents a cursor to a kvsm increment/transaction

```C
struct kvsm_cursor {
 const struct kvsm *ctx;
 PALLOC_OFFSET      parent;
 PALLOC_OFFSET      offset;
 uint64_t           increment;
};
```

</details>

### Methods

<details>
  <summary>struct kvsm * kvsm_open(filename, isBlockDev)</summary>

  Initializes a new `struct kvsm`, handling creating the file if needed.
  Returns a new descriptor or `NULL` on failure.

```C
struct kvsm * kvsm_open(const char *filename, const int isBlockDev);
```

</details>
### Example

This library includes [kvsmctl](util/kvsmctl.c) as an example program making
use of this library.

Many bugs were caught implementing and playing with it, but feel free to
[open an issue](https://github.com/finwo/kvsm.c/issues) when you encounter
something unexpected.
