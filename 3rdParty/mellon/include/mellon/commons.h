#ifndef FUTURES_COMMONS_H
#define FUTURES_COMMONS_H

#include <utility>

#ifndef FUTURES_EMPTY_BASE
#ifdef _MSC_VER
#define FUTURES_EMPTY_BASE __declspec(empty_bases)
#else
#define FUTURES_EMPTY_BASE
#endif
#endif

namespace mellon {

struct default_tag {};

template <typename T, typename Tag>
struct future;
template <typename T, typename Tag>
struct promise;
template <typename T, typename F, typename R, typename Tag>
struct future_temporary;

template <typename T>
struct is_future : std::false_type {};
template <typename T, typename Tag>
struct is_future<future<T, Tag>> : std::true_type {};
template <typename T>
inline constexpr auto is_future_v = is_future<T>::value;

template <typename T>
struct is_future_temporary : std::false_type {};
template <typename T, typename F, typename R, typename Tag>
struct is_future_temporary<future_temporary<T, F, R, Tag>> : std::true_type {};
template <typename T>
inline constexpr auto is_future_temporary_v = is_future_temporary<T>::value;

template <typename T>
inline constexpr auto is_future_like_v = is_future_v<T> || is_future_temporary_v<T>;

/**
 * Create a new pair of init_future and promise with value type `T`.
 * @tparam T value type
 * @return pair of init_future and promise.
 * @throws std::bad_alloc
 */
template <typename T, typename Tag = default_tag>
auto make_promise() -> std::pair<future<T, Tag>, promise<T, Tag>>;

template <typename T, typename Tag>
struct promise_type_based_extension {};

template <typename T, template <typename> typename F, typename Tag>
struct future_type_based_extensions;

struct yes_i_know_that_this_call_will_block_t {};
inline constexpr yes_i_know_that_this_call_will_block_t yes_i_know_that_this_call_will_block;

}  // namespace mellon

#endif  // FUTURES_COMMONS_H
