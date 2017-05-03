// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

// Make sure `assert` always triggers an assertion
#ifdef NDEBUG
#   undef NDEBUG
#endif

//! [full]
#include <boost/hana.hpp>

#include <boost/any.hpp>
#include <cassert>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <utility>
namespace hana = boost::hana;

//! [cases]
template <typename T>
auto case_ = [](auto f) {
  return hana::make_pair(hana::type_c<T>, f);
};

struct default_t;
auto default_ = case_<default_t>;
//! [cases]

//! [process]
template <typename Any, typename Default>
auto process(Any&, std::type_index const&, Default& default_) {
  return default_();
}

template <typename Any, typename Default, typename Case, typename ...Rest>
auto process(Any& a, std::type_index const& t, Default& default_,
             Case& case_, Rest& ...rest)
{
  using T = typename decltype(+hana::first(case_))::type;
  return t == typeid(T) ? hana::second(case_)(*boost::unsafe_any_cast<T>(&a))
                        : process(a, t, default_, rest...);
}
//! [process]

//! [switch_]
template <typename Any>
auto switch_(Any& a) {
  return [&a](auto ...cases_) {
    auto cases = hana::make_tuple(cases_...);

    auto default_ = hana::find_if(cases, [](auto const& c) {
      return hana::first(c) == hana::type_c<default_t>;
    });
    static_assert(default_ != hana::nothing,
      "switch is missing a default_ case");

    auto rest = hana::filter(cases, [](auto const& c) {
      return hana::first(c) != hana::type_c<default_t>;
    });

    return hana::unpack(rest, [&](auto& ...rest) {
      return process(a, a.type(), hana::second(*default_), rest...);
    });
  };
}
//! [switch_]
//! [full]


int main() {
using namespace std::literals;

{

//! [usage]
boost::any a = 'x';
std::string r = switch_(a)(
  case_<int>([](auto i) { return "int: "s + std::to_string(i); }),
  case_<char>([](auto c) { return "char: "s + std::string{c}; }),
  default_([] { return "unknown"s; })
);

assert(r == "char: x"s);
//! [usage]

}{

//! [result_inference]
boost::any a = 'x';
auto r = switch_(a)(
  case_<int>([](auto) -> int { return 1; }),
  case_<char>([](auto) -> long { return 2l; }),
  default_([]() -> long long { return 3ll; })
);

// r is inferred to be a long long
static_assert(std::is_same<decltype(r), long long>{}, "");
assert(r == 2ll);
//! [result_inference]

}

}
