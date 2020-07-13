#ifndef DESERIALIZER_HINTS_H
#define DESERIALIZER_HINTS_H
#include <type_traits>

#include "gadgets.h"
#include "types.h"
#include "vpack-types.h"

namespace deserializer::hints {

template<typename... Hs>
struct hint_list {
  constexpr static auto length = sizeof...(Hs);
  using state_type = std::tuple<typename Hs::state_type...>;
};

using hint_list_empty = hint_list<>;

template<const char N[]>
struct has_field {
  constexpr static auto name = N;
  using state_type = ::deserializer::slice_type;
};

template<const char N[], typename V>
struct has_field_with_value {
  constexpr static auto name = N;
  using value = V;
  using state_type = ::deserializer::slice_type;
};

struct empty_hint {
  using state_type = unit_type;
};

struct is_object : public empty_hint {};
struct is_string : public empty_hint {};

template<typename, typename>
struct hint_list_contains;

template<typename H, typename... Hs>
struct hint_list_contains<H, hint_list<Hs...>> {
  constexpr static auto value = (std::is_same_v<H, Hs> || ...);
};

template<typename H, typename... Hs>
constexpr const bool hint_list_contains_v = hint_list_contains<H, Hs...>::value;

template<typename H>
constexpr const bool hint_is_object = hint_list_contains_v<is_object, H>;

template<const char N[], typename H>
constexpr const bool hint_has_key = hint_list_contains_v<has_field<N>, H>;

template<typename, typename>
struct hint_list_state;
template<typename H, typename... Hs>
struct hint_list_state<H, hint_list<Hs...>> {

  constexpr static auto index = detail::gadgets::index_of_type_v<H, Hs...>;
  using tuple_type = typename hint_list<Hs...>::state_type;

  template<typename S>
  static auto get(S&& s) {
    static_assert(std::is_same_v<std::decay_t<S>, tuple_type>);
    return std::get<index>(std::forward<S>(s));
  }
};

}


#endif  // DESERIALIZER_HINTS_H
