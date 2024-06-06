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

Well, I wanted something simple that I can embed into other applications. This
library is designed to be simple, not to break any records.

### How

This library makes use of [palloc](https://github.com/finwo/palloc.c) to handle
blob allocations on a file or block device. Each blob represents a transaction.

From there, a transaction contains a parent reference, a time-based identifier
and a list of key-value pairs.

This in turn allows for out-of-order insertion of transactions, time-based
compaction of older transactions, and repairing of references if something went
wrong.
