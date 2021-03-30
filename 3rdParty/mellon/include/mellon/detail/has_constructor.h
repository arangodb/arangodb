#ifndef FUTURES_HAS_CONSTRUCTOR_H
#define FUTURES_HAS_CONSTRUCTOR_H

#include <utility>

#include "../commons.h"

namespace mellon::detail {
template <typename Tag, typename T, template <typename> typename F, typename Ts, typename = void>
struct has_constructor : std::false_type {};
template <typename Tag, typename T, template <typename> typename F, typename... Ts>
struct has_constructor<Tag, T, F, std::tuple<Ts...>,
                       std::void_t<decltype(future_type_based_extensions<T, F, Tag>::construct(std::declval<Ts>()...))>>
    : std::true_type {};
template <typename Tag, typename T, template <typename> typename F, typename... Ts>
inline constexpr auto has_constructor_v =
    has_constructor<Tag, T, F, std::tuple<Ts...>>::value;
}  // namespace mellon::detail

#endif  // FUTURES_HAS_CONSTRUCTOR_H
