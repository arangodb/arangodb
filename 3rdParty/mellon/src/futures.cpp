#include "mellon/futures.h"

using namespace mellon;

detail::invalid_pointer_type detail::invalid_pointer_inline_value;
detail::invalid_pointer_type detail::invalid_pointer_future_abandoned;
detail::invalid_pointer_type detail::invalid_pointer_promise_abandoned;
detail::invalid_pointer_type detail::invalid_pointer_promise_fulfilled;

const char* promise_abandoned_error::what() const noexcept {
  return "promise abandoned";
}
