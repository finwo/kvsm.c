#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#include "finwo/assert.h"
#include "rxi/log.h"

#include "src/kvsm.h"

void test_kvsm_regular() {
  /*struct kvsm *ctx;*/
  /**/
  /*ctx = kvsm_open("test.db", 0);*/
  /*ASSERT("Opening a file returns a context", ctx != NULL);*/
  /*ASSERT("Closing a file context returns OK", kvsm_close(ctx) == KVSM_OK);*/
  /**/
  /*ctx = kvsm_open(NULL, 0);*/
  /*ASSERT("Opening a NULL returns no context", ctx == NULL);*/
  /*ASSERT("Closing a NULL context returns ERROR", kvsm_close(ctx) != KVSM_OK);*/
  /**/
}

int main() {

  // Seed random
  unsigned int seed;
  FILE* urandom = fopen("/dev/urandom", "r");
  fread(&seed, sizeof(int), 1, urandom);
  fclose(urandom);
  srand(seed);

  // No verbose logging here
  log_set_level(LOG_FATAL);

  // Run the actual tests
  RUN(test_kvsm_regular);
  /*RUN(test_mindex_structs);*/
  return TEST_REPORT();
}
