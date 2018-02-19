#include "tests_shared.hpp"
#include "utils/fst_string_weight.h"
#include <string>

TEST(fst_string_weight_tests, check_size) {
  std::stringstream ss;

  fst::StringLeftWeight<char> w0;
  w0.PushBack(1);
  w0.PushBack(2);
  w0.PushBack(3);
  w0.PushBack(4);
  w0.PushBack(5);
  w0.Write(ss);

  ss.seekg(0);
  fst::StringLeftWeight<char> w1;
  w1.Read(ss);
  ASSERT_EQ(w0, w1);
}
