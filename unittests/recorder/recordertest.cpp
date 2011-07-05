#include "gtest/gtest.h"
#include "recorder/runtime/log.h"

TEST(recordertest, log) {
  int insid = 33;
  void *addr = (void*)0xdeadbeef;
  long long data = 12345;

  tern_log_init();
  tern_log(insid, addr, data);

  // TODO: create some real unitests
  EXPECT_EQ(insid, 33);
}
