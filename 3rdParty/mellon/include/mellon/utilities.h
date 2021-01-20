#ifndef FUTURES_UTILITIES_H
#define FUTURES_UTILITIES_H
#include <atomic>
#include <bitset>
#include <tuple>
#include "futures.h"

namespace mellon {
namespace detail {
template <typename...>
struct collect_tuple_context_impl;

template <typename... Ts, std::size_t... Is, typename Tag>
struct collect_tuple_context_impl<Tag, std::index_sequence<Is...>, Ts...>
    : detail::box<Ts, Is>... {
  using tuple_type = std::tuple<Ts...>;
  static constexpr auto fan_in_degree = sizeof...(Ts);

  ~collect_tuple_context_impl() noexcept {
    std::move(out_promise).fulfill(detail::box<Ts, Is>::cast_move()...);
    (detail::box<Ts, Is>::destroy(), ...);
  }

  template <std::size_t I, typename... Args, typename T = std::tuple_element_t<I, tuple_type>>
  void fulfill(Args&&... args) noexcept {
    static_assert(I < fan_in_degree);
    static_assert(std::is_nothrow_constructible_v<T, Args...>);
    detail::box<std::tuple_element_t<I, tuple_type>, I>::emplace(std::forward<Args>(args)...);
  }

  explicit collect_tuple_context_impl(promise<tuple_type, Tag> p)
      : out_promise(std::move(p)) {}

 private:
  promise<tuple_type, Tag> out_promise;
};

template <typename Tag, typename... Ts>
using collect_tuple_context =
    collect_tuple_context_impl<Tag, std::index_sequence_for<Ts...>, Ts...>;

template <typename Tag, typename... Fs, std::size_t... Is, typename R = std::tuple<future_value_type_t<Fs>...>>
auto collect(std::index_sequence<Is...>, Fs&&... fs) -> future<R, Tag> {
  auto&& [f, p] = make_promise<R, Tag>();
  auto ctx = std::make_shared<detail::collect_tuple_context<Tag, future_value_type_t<Fs>...>>(
      std::move(p));

  // TODO this is not as good as it could be.
  // We have two allocations, one for the context and one for the promise.
  // Third, we use a shared pointer that has more overhead than necessary.
  (std::invoke([&] {
    std::move(fs).finally([ctx](future_value_type_t<Fs>&& t) noexcept {
      ctx->template fulfill<Is>(std::move(t));
    });
  }),
      ...);

  return std::move(f);
}

template <typename Tag, typename... Fs, std::size_t... Is, typename R = std::tuple<future_value_type_t<Fs>...>>
auto collect(std::index_sequence<Is...> is, std::tuple<Fs...>&& fs) -> future<R, Tag> {
  return collect<Tag>(is, std::get<Is>(std::move(fs))...);
}

}  // namespace detail

/**
 * Collect creates a new my_future that awaits all given mellon and completes
 * when all mellon have received their value. It then returns a tuple
 * containing all the individual values.
 *
 * @tparam Ts value types of the mellon (deduced)
 * @param fs Futures that are collected.
 * @return A new my_future that returns a tuple.
 */
template <typename... Fs, typename Tag, typename R = std::tuple<Fs...>>
auto collect(future<Fs, Tag>&&... fs) -> future<R, Tag> {
  // TODO maybe we want to extend this function to allow to accept temporary objects
  return detail::collect<Tag>(std::index_sequence_for<Fs...>{}, std::move(fs)...);
}

template <typename... Fs, typename Tag, typename R = std::tuple<Fs...>>
auto collect(std::tuple<future<Fs, Tag>...>&& tfs) -> future<R, Tag> {
  // TODO maybe we want to extend this function to allow to accept temporary objects
  return detail::collect<Tag>(std::index_sequence_for<Fs...>{}, std::move(tfs));
}

/**
 * Collects all mellon in the range [begin, end). The result is new my_future
 * returning a `std::vector<T>`. The results might appear in a different order
 * then they appeared in the input range. If a promise is abandoned, its result
 * is omitted from the result vector.
 * @tparam InputIt
 * @param begin
 * @param end
 * @return
 */
template <typename InputIt, typename V = typename std::iterator_traits<InputIt>::value_type,
          std::enable_if_t<is_future_v<V>, int> = 0,
          typename B = future_value_type_t<V>, typename R = std::vector<B>, typename Tag = future_tag_type_t<V>>
auto collect(InputIt begin, InputIt end) -> future<R, Tag> {
  auto&& [f, p] = make_promise<R, Tag>();

  // TODO this does two allocations
  struct context {
    ~context() {
      R v;  // FIXME do not copy every entry into a vector
      v.reserve(size);
      for (std::size_t i = 0; i < size; i++) {
        v.emplace_back(result[i].cast_move());
        result[i].destroy();
      }

      std::move(out_promise).fulfill(std::move(v));
    }
    explicit context(promise<R, Tag> p, std::size_t size)
        : out_promise(std::move(p)),
          size(size),
          result(std::make_unique<detail::box<B>[]>(size)) {}
    promise<R, Tag> out_promise;
    std::size_t size;
    std::unique_ptr<detail::box<B>[]> result;
  };

  // FIXME: do we actually need `distance` here?
  auto size = std::distance(begin, end);
  auto ctx = std::make_shared<context>(std::move(p), size);
  for (std::size_t i = 0; begin != end; ++begin, ++i) {
    std::move(*begin).finally(
        [i, ctx](B&& v) noexcept { ctx->result[i].emplace(std::move(v)); });
  }
  return std::move(f);
}
}  // namespace mellon

#endif  // FUTURES_UTILITIES_H
