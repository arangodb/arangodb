#include <mellon/futures.h>

using namespace mellon;
using namespace expect;

struct default_constructible {
  default_constructible() = default;
};

struct non_default_constructible {
  non_default_constructible() = delete;
};


template<typename T>
struct convertible_to_T {
  operator T() noexcept;
};

template<typename T, typename Tag = mellon::default_tag>
struct convertible_asserts : std::true_type {
  static_assert(std::is_convertible_v<future<convertible_to_T<T>, Tag>, future<T, Tag>>);
  static_assert(std::is_convertible_v<future<expected<convertible_to_T<T>>, Tag>, future<expected<T>, Tag>>);

  static_assert(std::is_same_v<future<T, Tag>, decltype(std::declval<future<convertible_to_T<T>, Tag>>().template as<T>().finalize())>);
};

template<typename T>
struct convertible_asserts_and_expected : convertible_asserts<T>, convertible_asserts<expected<T>> {};

static_assert(convertible_asserts_and_expected<int>::value);
static_assert(convertible_asserts_and_expected<double>::value);
static_assert(convertible_asserts_and_expected<std::string>::value);
static_assert(convertible_asserts_and_expected<std::shared_ptr<int>>::value);
