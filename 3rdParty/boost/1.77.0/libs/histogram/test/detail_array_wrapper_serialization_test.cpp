// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/detail/array_wrapper.hpp>
#include <sstream>
#include <vector>
#include "std_ostream.hpp"
#include "throw_exception.hpp"

namespace dtl = boost::histogram::detail;
namespace ba = boost::archive;

template <class T>
struct dummy_array_wrapper {
  T* ptr;
  std::size_t size;
  template <class Archive>
  void serialize(Archive& ar, unsigned /* version */) {
    for (auto&& x : dtl::make_span(ptr, size)) ar& x;
  }
};

template <class OArchive, class IArchive>
void run_tests() {
  std::vector<int> v = {{1, 2, 3}};

  std::stringstream os1;
  {
    OArchive oa(os1);
    auto w = dtl::make_array_wrapper(v.data(), v.size());
    oa << w;
  }

  std::ostringstream os2;
  {
    OArchive oa(os2);
    auto w = dummy_array_wrapper<int>{v.data(), v.size()};
    oa << w;
  }

  BOOST_TEST_EQ(os1.str(), os2.str());

  std::vector<int> v2(3, 0);
  {
    IArchive ia(os1);
    auto w = dtl::make_array_wrapper(v2.data(), v2.size());
    ia >> w;
  }

  BOOST_TEST_EQ(v, v2);
}

int main() {
  BOOST_TEST(dtl::has_array_optimization<ba::binary_oarchive>::value);
  BOOST_TEST(dtl::has_array_optimization<ba::binary_iarchive>::value);
  BOOST_TEST_NOT(dtl::has_array_optimization<ba::text_oarchive>::value);
  BOOST_TEST_NOT(dtl::has_array_optimization<ba::text_iarchive>::value);

  run_tests<ba::binary_oarchive, ba::binary_iarchive>();
  run_tests<ba::text_oarchive, ba::text_iarchive>();

  return boost::report_errors();
}
