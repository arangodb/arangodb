// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

// Make sure `assert` always triggers an assertion
#ifdef NDEBUG
#   undef NDEBUG
#endif

#include <boost/fusion/include/comparison.hpp>
#include <boost/fusion/include/make_vector.hpp>
#include <boost/fusion/include/transform.hpp>
#include <boost/fusion/include/vector.hpp>

#include <boost/mpl/equal.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
namespace fusion = boost::fusion;
namespace mpl = boost::mpl;
using namespace std::literals;


int main() {

{

//! [runtime]
auto f = [](int i) -> std::string {
  return std::to_string(i * i);
};

std::vector<int> ints{1, 2, 3, 4};
std::vector<std::string> strings;
std::transform(ints.begin(), ints.end(), std::back_inserter(strings), f);

assert((strings == std::vector<std::string>{"1", "4", "9", "16"}));
//! [runtime]

}{

//! [heterogeneous]
auto to_string = [](auto t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
};

fusion::vector<int, std::string, float> seq{1, "abc", 3.4f};
fusion::vector<std::string, std::string, std::string>
  strings = fusion::transform(seq, to_string);

assert(strings == fusion::make_vector("1"s, "abc"s, "3.4"s));
//! [heterogeneous]

}

}

//! [type-level]
template <typename T>
struct add_const_pointer {
  using type = T const*;
};

using types = mpl::vector<int, char, float, void>;
using pointers = mpl::transform<types, add_const_pointer<mpl::_1>>::type;

static_assert(mpl::equal<
  pointers,
  mpl::vector<int const*, char const*, float const*, void const*>
>::value, "");
//! [type-level]
