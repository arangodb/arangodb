// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/tuple.hpp>

#include <cstddef>
#include <iostream>
#include <sstream>
namespace hana = boost::hana;


//! [setup]
template <typename Tag>
struct print_impl {
  template <typename X>
  static void apply(std::ostream&, X const&) {
    // possibly some default implementation
  }
};

template <typename X>
void print(std::ostream& os, X x) {
  using Tag = typename hana::tag_of<X>::type;
  print_impl<Tag>::apply(os, x);
}
//! [setup]

//! [vector]
struct vector_tag;

struct vector0 {
  using hana_tag = vector_tag;
  static constexpr std::size_t size = 0;
};

template <typename T1>
struct vector1 {
  T1 t1;
  using hana_tag = vector_tag;
  static constexpr std::size_t size = 1;

  template <typename Index>
  auto const& operator[](Index i) const {
    static_assert(i == 0u, "index out of bounds");
    return t1;
  }
};

template <typename T1, typename T2>
struct vector2 {
  T1 t1; T2 t2;
  using hana_tag = vector_tag;
  static constexpr std::size_t size = 2;

  // Using Hana as a backend to simplify the example.
  template <typename Index>
  auto const& operator[](Index i) const {
    return *hana::make_tuple(&t1, &t2)[i];
  }
};

// and so on...
//! [vector]

//! [customize]
template <>
struct print_impl<vector_tag> {
  template <typename vectorN>
  static void apply(std::ostream& os, vectorN xs) {
    auto N = hana::size_c<vectorN::size>;

    os << "[";
    N.times.with_index([&](auto i) {
      os << xs[i];
      if (i != N - hana::size_c<1>) os << ", ";
    });
    os << "]";
  }
};
//! [customize]

#if 0
//! [customize-when]
template <typename Tag>
struct print_impl<Tag, hana::when<Tag represents some kind of sequence>> {
  template <typename Seq>
  static void apply(std::ostream& os, Seq xs) {
    // Some implementation for any sequence
  }
};
//! [customize-when]
#endif

int main() {
  {
    std::stringstream ss;
    vector0 v0;
    print(ss, v0);
    BOOST_HANA_RUNTIME_CHECK(ss.str() == "[]");
  }

  {
    std::stringstream ss;
    vector1<int> v1{1};
    print(ss, v1);
    BOOST_HANA_RUNTIME_CHECK(ss.str() == "[1]");
  }

  {
    std::stringstream ss;
    vector2<int, char> v2{1, 'x'};
    print(ss, v2);
    BOOST_HANA_RUNTIME_CHECK(ss.str() == "[1, x]");
  }
}

namespace old_way {
//! [old_way]
void print(std::ostream& os, vector0)
{ os << "[]"; }

template <typename T1>
void print(std::ostream& os, vector1<T1> v)
{ os << "[" << v.t1 << "]"; }

template <typename T1, typename T2>
void print(std::ostream& os, vector2<T1, T2> v)
{ os << "[" << v.t1 << ", " << v.t2 << "]"; }

// and so on...
//! [old_way]
}

namespace preconditions {
//! [preconditions]
template <typename X>
void print(std::ostream& os, X x) {
  // **** check some precondition ****

  // The precondition only has to be checked here; implementations
  // can assume their arguments to always be sane.

  using Tag = typename hana::tag_of<X>::type;
  print_impl<Tag>::apply(os, x);
}
//! [preconditions]
}

namespace function_objects {
//! [function_objects]
// Defining a function object is only needed once and implementations do not
// have to worry about static initialization and other painful tricks.
struct print_t {
  template <typename X>
  void operator()(std::ostream& os, X x) const {
    using Tag = typename hana::tag_of<X>::type;
    print_impl<Tag>::apply(os, x);
  }
};
constexpr print_t print{};
//! [function_objects]

static_assert(sizeof(print) || true, "remove unused variable print warning");
}
