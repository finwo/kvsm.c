#include "rxi/log.h"

#include "kvsm.h"

int main() {
  log_set_level(LOG_TRACE);

  struct kvsm *ctx = kvsm_open("pizza.db", 0);
  struct kvsm_transaction *tx = kvsm_transaction_init(ctx);

  kvsm_transaction_set(tx,
      &((struct buf){ .data = "foo", .len = 3 }),
      &((struct buf){ .data = "bar", .len = 3 })
  );

  kvsm_transaction_commit(tx);
  kvsm_transaction_free(tx);

  kvsm_close(ctx);



  /* log_info("Writing mock data: foo = bar..."); */
  /* tx = kvsm_transaction_init(); */
  /* kvsm_transaction_set(tx, &((struct buf){ .data = "foo", .len = 3 }), &((struct buf){ .data = "bar", .len = 3 })); */
  /* kvsm_transaction_store(kvsm_state, tx); */
  /* kvsm_transaction_free(tx); */
  /* log_info("OK\n"); */

  /* log_info("Reading mock data: foo = bar..."); */
  /* tx = kvsm_transaction_init(); */
  /* struct buf *received = kvsm_transaction_get(tx, &((struct buf){ .data = "xxx", .len = 3 })); */
  /* kvsm_transaction_free(tx); */
  /* log_info("OK\n"); */

  return 42;
}
