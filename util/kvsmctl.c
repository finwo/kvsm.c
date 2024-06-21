#include <stdio.h>
#include <strings.h>
#include <unistd.h>

#include "rxi/log.h"

#include "kvsm.h"

void usage_global(char **argv) {
  printf("\n");
  printf("Usage: %s [global opts] command [command opts]\n", argv[0]);
  printf("\n");
  printf("Global options\n");
  printf("  -h           Show this usage\n");
  printf("  -f filename  Set database file to operate on\n");
  printf("  -v level     Set verbosity level (fatal,error,warn,info,debug,trace)\n");
  printf("\n");
  printf("Commands\n");
  printf("  TODO\n");
  printf("\n");
}

int main(int argc, char **argv) {
  log_set_level(LOG_TRACE);
  char *filename = NULL;
  char *command  = NULL;

  // Parse global options
  int c;
  while((c = getopt(argc, argv, "hf:v:")) != -1) {
    switch(c) {
      case 'h':
        usage_global(argv);
        return 0;
      case 'f':
        filename = optarg;
        break;
      case 'v':
        if (0) {
          // Intentionally empty
        } else if (!strcasecmp(optarg, "trace")) {
          log_set_level(LOG_TRACE);
        } else if (!strcasecmp(optarg, "debug")) {
          log_set_level(LOG_DEBUG);
        } else if (!strcasecmp(optarg, "info")) {
          log_set_level(LOG_INFO);
        } else if (!strcasecmp(optarg, "warn")) {
          log_set_level(LOG_WARN);
        } else if (!strcasecmp(optarg, "error")) {
          log_set_level(LOG_ERROR);
        } else if (!strcasecmp(optarg, "fatal")) {
          log_set_level(LOG_FATAL);
        } else {
          log_fatal("Unknown log level: %s", optarg);
          return 1;
        }
        break;
      default:
        log_fatal("illegal option", c);
        return 1;
    }
  }
  if (optind < argc) {
    command = argv[optind++];
  }
  if (!command) {
    log_fatal("No command given");
  }


  /*struct kvsm *ctx = kvsm_open("pizza.db", 0);*/
  /*struct kvsm_transaction *tx = kvsm_transaction_init(ctx);*/

  /*kvsm_transaction_set(tx,*/
  /*    &((struct buf){ .data = "foo", .len = 3 }),*/
  /*    &((struct buf){ .data = "bar", .len = 3 })*/
  /*);*/

  /*kvsm_transaction_commit(tx);*/
  /*kvsm_transaction_free(tx);*/

  /*kvsm_close(ctx);*/



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
