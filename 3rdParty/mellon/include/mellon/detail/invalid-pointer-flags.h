#ifndef FUTURES_INVALID_POINTER_FLAGS_H
#define FUTURES_INVALID_POINTER_FLAGS_H

namespace mellon::detail {
struct invalid_pointer_type {};
extern invalid_pointer_type invalid_pointer_inline_value;
extern invalid_pointer_type invalid_pointer_future_abandoned;
extern invalid_pointer_type invalid_pointer_promise_abandoned;
extern invalid_pointer_type invalid_pointer_promise_fulfilled;

#define FUTURES_INVALID_POINTER_INLINE_VALUE(Tag, T) \
  reinterpret_cast<::mellon::detail::continuation_base<Tag, T>*>(&detail::invalid_pointer_inline_value)
#define FUTURES_INVALID_POINTER_FUTURE_ABANDONED(T) \
  reinterpret_cast<::mellon::detail::continuation<T>*>(&detail::invalid_pointer_future_abandoned)
#define FUTURES_INVALID_POINTER_PROMISE_ABANDONED(T) \
  reinterpret_cast<::mellon::detail::continuation<T>*>(&detail::invalid_pointer_promise_abandoned)
#define FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T) \
  reinterpret_cast<::mellon::detail::continuation<T>*>(&detail::invalid_pointer_promise_fulfilled)

}  // namespace mellon::detail

#endif  // FUTURES_INVALID_POINTER_FLAGS_H
