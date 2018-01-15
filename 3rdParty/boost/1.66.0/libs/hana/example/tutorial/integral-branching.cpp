// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
namespace hana = boost::hana;



namespace ns1 {
//! [make_unique.if_]
template <typename T, typename ...Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return hana::if_(std::is_constructible<T, Args...>{},
    [](auto&& ...x) { return std::unique_ptr<T>(new T(std::forward<Args>(x)...)); },
    [](auto&& ...x) { return std::unique_ptr<T>(new T{std::forward<Args>(x)...}); }
  )(std::forward<Args>(args)...);
}
//! [make_unique.if_]
}


namespace ns2 {
//! [make_unique.eval_if]
template <typename T, typename ...Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return hana::eval_if(std::is_constructible<T, Args...>{},
    [&](auto _) { return std::unique_ptr<T>(new T(std::forward<Args>(_(args))...)); },
    [&](auto _) { return std::unique_ptr<T>(new T{std::forward<Args>(_(args))...}); }
  );
}
//! [make_unique.eval_if]
}

struct Student {
  std::string name;
  int age;
};

int main() {
  {
    std::unique_ptr<int> a = ns1::make_unique<int>(3);
    std::unique_ptr<Student> b = ns1::make_unique<Student>("Bob", 25);
  }
  {
    std::unique_ptr<int> a = ns2::make_unique<int>(3);
    std::unique_ptr<Student> b = ns2::make_unique<Student>("Bob", 25);
  }
}
