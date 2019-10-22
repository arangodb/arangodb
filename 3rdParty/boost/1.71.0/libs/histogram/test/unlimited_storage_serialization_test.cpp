// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/assert.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/serialization.hpp>
#include <boost/histogram/unlimited_storage.hpp>
#include <memory>
#include <sstream>
#include "throw_exception.hpp"
#include "utility_serialization.hpp"

using unlimited_storage_type = boost::histogram::unlimited_storage<>;

using namespace boost::histogram;

template <typename T>
unlimited_storage_type prepare(std::size_t n, const T x) {
  std::unique_ptr<T[]> v(new T[n]);
  std::fill(v.get(), v.get() + n, static_cast<T>(0));
  v.get()[0] = x;
  return unlimited_storage_type(n, v.get());
}

template <typename T>
unlimited_storage_type prepare(std::size_t n) {
  return unlimited_storage_type(n, static_cast<T*>(nullptr));
}

template <typename T>
void run_test(const std::string& filename) {
  const auto a = prepare(1, T(1));
  print_xml(filename, a);
  unlimited_storage_type b;
  BOOST_TEST(!(a == b));
  load_xml(filename, b);
  BOOST_TEST(a == b);
}

int main(int argc, char** argv) {
  BOOST_ASSERT(argc == 2);

  run_test<uint8_t>(join(argv[1], "unlimited_storage_serialization_test_u8.xml"));
  run_test<uint16_t>(join(argv[1], "unlimited_storage_serialization_test_u16.xml"));
  run_test<uint32_t>(join(argv[1], "unlimited_storage_serialization_test_u32.xml"));
  run_test<uint64_t>(join(argv[1], "unlimited_storage_serialization_test_u64.xml"));
  run_test<unlimited_storage_type::large_int>(
      join(argv[1], "unlimited_storage_serialization_test_large_int.xml"));
  run_test<double>(join(argv[1], "unlimited_storage_serialization_test_double.xml"));

  return boost::report_errors();
}
