#ifndef ARANGODB3_TEMPLATE_STUFF_H
#define ARANGODB3_TEMPLATE_STUFF_H

#ifndef TRI_ASSERT
#define TRI_ASSERT(x) \
  if (!(x)) {         \
    std::abort();     \
  }
#endif

// FIXME: This needs to go into support code somehwere.
namespace detail {
template <class>
inline constexpr bool always_false_v = false;
template <typename... Ts, std::size_t... Is>
auto unpackTuple(VPackArrayIterator& iter, std::index_sequence<Is...>) {
  std::tuple<Ts...> result;
  (
      [&result](VPackSlice slice) {
        TRI_ASSERT(!slice.isNone());
        auto& value = std::get<Is>(result);
        if constexpr (std::is_same_v<Ts, bool>) {
          TRI_ASSERT(slice.isBool());
          value = slice.getBool();
        } else if constexpr (std::is_integral_v<Ts>) {
          TRI_ASSERT(slice.template isNumber<Ts>());
          value = slice.template getNumericValue<Ts>();
        } else if constexpr (std::is_same_v<Ts, double>) {
          TRI_ASSERT(slice.isDouble());
          value = slice.getDouble();
        } else if constexpr (std::is_same_v<Ts, std::string>) {
          TRI_ASSERT(slice.isString());
          value = slice.copyString();
        } else if constexpr (std::is_same_v<Ts, std::string_view>) {
          TRI_ASSERT(slice.isString());
          value = slice.stringView();
        } else if constexpr (std::is_same_v<Ts, VPackStringRef>) {
          TRI_ASSERT(slice.isString());
          value = slice.stringRef();
        } else if constexpr (std::is_same_v<Ts, VPackSlice>) {
          value = slice;
        } else {
          static_assert(always_false_v<Ts>, "Unhandled value type requested");
        }
      }(*(iter++)),
      ...);
  return result;
}
}  // namespace detail
/// @brief unpacks an array as tuple. Use like this: auto&& [a, b, c] = unpack<size_t, std::string, double>(slice);
template <typename... Ts>
static std::tuple<Ts...> unpackTuple(VPackSlice slice) {
  VPackArrayIterator iter(slice);
  return detail::unpackTuple<Ts...>(iter, std::index_sequence_for<Ts...>{});
}
template <typename... Ts>
static std::tuple<Ts...> unpackTuple(VPackArrayIterator& iter) {
  return detail::unpackTuple<Ts...>(iter, std::index_sequence_for<Ts...>{});
}


#endif  // ARANGODB3_TEMPLATE_STUFF_H
