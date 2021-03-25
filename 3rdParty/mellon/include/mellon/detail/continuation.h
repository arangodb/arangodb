#ifndef FUTURES_CONTINUATION_H
#define FUTURES_CONTINUATION_H

#ifdef FUTURES_COUNT_ALLOC
#include <cmath>
#endif

#ifdef MELLON_RECORD_PENDING_OBJECTS
#include <vector>
#include <unordered_set>
#endif

#include "../traits.h"
#include "gadgets.h"
#include "invalid-pointer-flags.h"

namespace mellon::detail {


#ifdef MELLON_RECORD_BACKTRACE
extern thread_local std::vector<std::string>* current_backtrace_ptr;
auto generate_backtrace_string() noexcept -> std::vector<std::string>;
#endif

template <typename Tag, typename T>
struct handler_helper {
  static T abandon_promise(std::vector<std::string>* bt = nullptr) noexcept {
#ifdef MELLON_RECORD_BACKTRACE
    current_backtrace_ptr = bt;
#endif
    return detail::tag_trait_helper<Tag>::template abandon_promise<T>();
  }

  template <typename U>
  static void abandon_future(U&& u, std::vector<std::string>* bt = nullptr) noexcept {
#ifdef MELLON_RECORD_BACKTRACE
    current_backtrace_ptr = bt;
#endif
    detail::tag_trait_helper<Tag>::template abandon_future<T>(std::forward<U>(u));
  }
};

template <typename T>
struct continuation;
template <typename Tag, typename T, std::size_t = tag_trait_helper<Tag>::finally_prealloc_size()>
struct continuation_base;
template <typename Tag, typename T>
struct continuation_start;
template <typename Tag, typename T, typename F, typename R>
struct continuation_step;
template <typename T, typename F, typename>
struct continuation_final;

template <typename Tag, typename T, typename... Args,
          std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
void fulfill_continuation(continuation_base<Tag, T>* base,
                          Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");
  base->emplace(std::forward<Args>(args)...);  // this can throw an exception

  // the remainder should be noexcept
  static_assert(std::is_nothrow_destructible_v<T>,
                "type should be nothrow destructible.");
  std::invoke([&]() noexcept {
    // (1) - this acquire-load synchronizes-with the release-CAS (4, 8, 10)
    continuation<T>* expected = base->_next.load(std::memory_order_acquire);
    if (expected != nullptr || 
        // (2) - the release-store synchronizes-with the acquire-(re)load (3, 4, 7, 8, 9, 10)
        //       the acquire-reload synchronizes-with the release-CAS (4, 8, 10)
        // Note: it would be sufficient to use memory_order_release for the success cases, but
        // since TSan does not support failure-orders, we use acq-rel to avoid false positives.
        !base->_next.compare_exchange_strong(expected, FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T),
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
      if (expected != FUTURES_INVALID_POINTER_FUTURE_ABANDONED(T)) {
        std::invoke(*expected, base->cast_move());
        // FIXME this is a recursive call. Build this a loop!
      } else {
        detail::tag_trait_helper<Tag>::assert_true(
            expected == FUTURES_INVALID_POINTER_FUTURE_ABANDONED(T),
            "invalid continuation state");
        detail::handler_helper<Tag, T>::abandon_future(base->cast_move(), base->get_backtrace());
        static_assert(std::is_nothrow_destructible_v<T>);
      }

      base->destroy();
      detail::tag_trait_helper<Tag>::release(base);
    }
  });
}

template <typename Tag, typename T>
void abandon_continuation(continuation_base<Tag, T>* base) noexcept {
  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");

#ifdef MELLON_RECORD_BACKTRACE
  {
      std::unique_lock guard(base->_abt_guard);
      base->_abt = generate_backtrace_string();
  }
#endif

  // (3) - this acquire-load synchronizes-with the release-CAS (2, 6)
  continuation<T>* expected = base->_next.load(std::memory_order_acquire);
  if (expected != nullptr ||
      // (4) - the release-store synchronizes-with the acquire-(re)load (1, 2, 5, 6)
      //       the acquire-reload synchronizes-with the release-CAS (2, 6)
      // Note: it would be sufficient to use memory_order_release for the success cases, but
      // since TSan does not support failure-orders, we use acq-rel to avoid false positives.
      !base->_next.compare_exchange_strong(expected, FUTURES_INVALID_POINTER_FUTURE_ABANDONED(T),
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    if (expected == FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {
      detail::handler_helper<Tag, T>::abandon_future(base->cast_move(), base->get_backtrace());
      static_assert(std::is_nothrow_destructible_v<T>);
      base->destroy();
    } else {
      detail::tag_trait_helper<Tag>::assert_true(expected == FUTURES_INVALID_POINTER_PROMISE_ABANDONED(T),
                                                 "invalid continuation state");
    }

    detail::tag_trait_helper<Tag>::release(base);
  }
}

template <typename Tag, typename T>
void abandon_promise(continuation_start<Tag, T>* base) noexcept {
  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");
#ifdef MELLON_RECORD_BACKTRACE
  {
    std::unique_lock guard(base->_abt_guard);
    base->_abt = generate_backtrace_string();
  }
#endif

  // (5) - this acquire-load synchronizes-with the release-CAS (4, 8, 10)
  continuation<T>* expected = base->_next.load(std::memory_order_acquire);
  if (expected != nullptr || 
      // (6) - the release-store synchronizes-with the acquire-(re)load (3, 4, 7, 8, 9, 10)
      //       the acquire-reload synchronizes-with the release-CAS (4, 8, 10)
      // Note: it would be sufficient to use memory_order_release for the success cases, but
      // since TSan does not support failure-orders, we use acq-rel to avoid false positives.
      !base->_next.compare_exchange_strong(expected, FUTURES_INVALID_POINTER_PROMISE_ABANDONED(T),
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    if (expected == FUTURES_INVALID_POINTER_FUTURE_ABANDONED(T)) {
      detail::tag_trait_helper<Tag>::release(base);  // we all agreed on not having this promise-future-chain
    } else {
      return fulfill_continuation<Tag>(base, detail::handler_helper<Tag, T>::abandon_promise(base->get_backtrace()));
    }
  }
}


#ifdef MELLON_RECORD_PENDING_OBJECTS
struct continuation_object_recorder;

struct continuation_rel_base {
  virtual ~continuation_rel_base() = default;
  virtual auto get_object_recorder_ptr() const ->  continuation_object_recorder const* = 0;
};

struct continuation_object_recorder {
  virtual ~continuation_object_recorder() = default;
  virtual auto get_tag_name() const -> std::string = 0;
  virtual auto get_value_type_name() const -> std::string = 0;
  virtual auto get_create_backtrace() const -> std::vector<std::string> const& = 0;
  virtual auto get_next_pointer() const -> continuation_rel_base* = 0;
};

extern std::unordered_set<continuation_object_recorder*> pending_objects;
extern std::mutex pending_objects_mutex;
void dump_pending_objects();

template <typename T, typename Tag>
struct continuation_object_recorder_impl : continuation_object_recorder {
  continuation_object_recorder_impl() {
    create_backtrace = generate_backtrace_string();
    std::unique_lock guard(pending_objects_mutex);
    pending_objects.insert(this);
  }

  ~continuation_object_recorder_impl() {
    std::unique_lock guard(pending_objects_mutex);
    pending_objects.erase(this);
  }

  auto get_tag_name() const -> std::string override {
    return typeid(Tag).name();
  }
  auto get_value_type_name() const -> std::string override {
    return typeid(T).name();
  }
  auto get_create_backtrace() const -> std::vector<std::string> const& override {
    return create_backtrace;
  }

 private:
  std::vector<std::string> create_backtrace;
};


#else
template<typename T, typename Tag>
struct continuation_object_recorder_impl {};
struct continuation_rel_base {};
#endif



template <typename T>
struct continuation : continuation_rel_base {
  virtual ~continuation() = default;
  virtual void operator()(T&&) noexcept = 0;
};

template <typename T, typename F, typename Deleter>
struct continuation_final final : continuation<T>,
                                  function_store<F>,
                                  continuation_object_recorder_impl<T, default_tag> {
  static_assert(std::is_nothrow_invocable_r_v<void, F, T>);
  template <typename G = F>
  explicit continuation_final(std::in_place_t, G&& f) noexcept(
      std::is_nothrow_constructible_v<function_store<F>, std::in_place_t, G>)
      : function_store<F>(std::in_place, std::forward<G>(f)) {}
  void operator()(T&& t) noexcept override {
    std::invoke(function_store<F>::function_self(), std::move(t));
    Deleter{}(this);
  }

#ifdef MELLON_RECORD_PENDING_OBJECTS
  auto get_object_recorder_ptr() const -> const continuation_object_recorder* override {
    return static_cast<const continuation_object_recorder*>(this);
  };

  auto get_next_pointer() const -> continuation_rel_base* override { return nullptr; };
#endif
};

template <typename Tag, typename T, std::size_t prealloc_size>
struct continuation_base : memory_buffer<prealloc_size>, box<T>, continuation_object_recorder_impl<T, Tag> {
  template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args...>, int> = 0>
  explicit continuation_base(std::in_place_t, Args&&... args) noexcept(
      std::is_nothrow_constructible_v<box<T>, std::in_place_t, Args...>)
      : box<T>(std::in_place, std::forward<Args>(args)...),
        _next(FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {}

  template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
  explicit continuation_base(std::in_place_t) noexcept
      : box<T>(std::in_place), _next(FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {}

  continuation_base() noexcept : box<T>() {}
  virtual ~continuation_base() = default;

  std::atomic<continuation<T>*> _next = nullptr;
#ifdef MELLON_RECORD_BACKTRACE
  std::vector<std::string> _abt; // backtrace for abandoned objects
  std::mutex _abt_guard;
  std::vector<std::string>* get_backtrace() { return &_abt; }
#else
  std::vector<std::string>* get_backtrace() { return nullptr; }
#endif

#ifdef MELLON_RECORD_PENDING_OBJECTS
  auto get_next_pointer() const -> continuation_rel_base* override { return _next.load(std::memory_order_relaxed); };
#endif
};

template <typename Tag, typename T>
struct continuation_start final : continuation_base<Tag, T> {};

template <typename Tag, typename T, typename F, typename R>
struct continuation_step final : continuation_base<Tag, R>,
                                 function_store<F>,
                                 continuation<T> {
  static_assert(std::is_nothrow_invocable_r_v<R, F, T&&>);

  template <typename G = F>
  explicit continuation_step(std::in_place_t, G&& f) noexcept(
      std::is_nothrow_constructible_v<function_store<F>, std::in_place_t, G>)
      : function_store<F>(std::in_place, std::forward<G>(f)) {}

  void operator()(T&& t) noexcept override {
    detail::fulfill_continuation<Tag>(this, std::invoke(function_store<F>::function_self(),
                                                        std::move(t)));
  }

#ifdef MELLON_RECORD_PENDING_OBJECTS
  auto get_object_recorder_ptr() const -> continuation_object_recorder const * override {
    return static_cast<continuation_object_recorder const *>(this);
  };
#endif
};

#ifdef MELLON_RECORD_PENDING_OBJECTS
auto generate_backtrace_string() noexcept -> std::vector<std::string>;
#endif

template <typename Tag, typename T, typename... Args>
auto allocate_frame_noexcept(Args&&... args) noexcept -> T* {
  static_assert(std::is_nothrow_constructible_v<T, Args...>,
                "type should be nothrow constructable");
  auto frame = detail::tag_trait_helper<Tag>::template allocate<T>(std::nothrow);
  detail::tag_trait_helper<Tag>::assert_true(frame != nullptr, "critical allocation failed");
  return new (frame) T(std::forward<Args>(args)...);
}

struct base_ptr_t {};
inline constexpr base_ptr_t base_ptr{};

template <typename Tag, typename T, typename F, typename R, typename G>
auto insert_continuation_step(continuation_base<Tag, T>* base, G&& f) noexcept
    -> future<R, Tag> {
  static_assert(std::is_nothrow_invocable_r_v<R, F, T&&>);
  static_assert(std::is_nothrow_destructible_v<T>);

  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");

  // (7) - this acquire-load synchronizes-with the release-CAS (2, 6)
  if (base->_next.load(std::memory_order_acquire) ==
      FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {
    // short path
    static_assert(std::is_nothrow_move_constructible_v<R>);
    auto fut = future<R, Tag>{std::in_place,
                              std::invoke(std::forward<G>(f), base->cast_move())};
    base->destroy();
    detail::tag_trait_helper<Tag>::release(base);
    return fut;
  }

  FUTURES_INC_ALLOC_COUNTER(number_of_step_usage);

  auto step = detail::allocate_frame_noexcept<Tag, continuation_step<Tag, T, F, R>>(
      std::in_place, std::forward<G>(f));
  continuation<T>* expected = nullptr;
  // (8) - the release-store synchronizes-with the acquire-(re)load (1, 2, 5, 6)
  //       the acquire-reload synchronizes-with the release-CAS (2, 6)
  // Note: it would be sufficient to use memory_order_release for the success cases, but
  // since TSan does not support failure-orders, we use acq-rel to avoid false positives.
  if (!base->_next.compare_exchange_strong(expected, step, std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    T value = std::invoke([&] {
      if (expected == FUTURES_INVALID_POINTER_PROMISE_ABANDONED(T)) {
        return detail::handler_helper<Tag, T>::abandon_promise(base->get_backtrace());
      } else {
        detail::tag_trait_helper<Tag>::assert_true(
            expected == FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T),
            "invalid continuation state");
        return base->cast_move();
      }
    });

    if constexpr (future<R, Tag>::is_value_inlined) {
      auto fut = future<R, Tag>{std::in_place,
                                std::invoke(step->function_self(), std::move(value))};
      base->destroy();
      detail::tag_trait_helper<Tag>::release(base);
      detail::tag_trait_helper<Tag>::release(step);
      return fut;
    } else {
      step->emplace(std::invoke(step->function_self(), std::move(value)));
      step->_next.store(FUTURES_INVALID_POINTER_PROMISE_FULFILLED(R), std::memory_order_relaxed);
      base->destroy();
      detail::tag_trait_helper<Tag>::release(base);
    }
  }

  return future<R, Tag>{detail::base_ptr, step};
}

template <typename Tag, typename T, typename F>
void insert_continuation_final(continuation_base<Tag, T>* base, F&& f) noexcept {
  static_assert(std::is_nothrow_invocable_r_v<void, F, T&&>);
  static_assert(std::is_nothrow_constructible_v<F, F>);
  static_assert(std::is_nothrow_destructible_v<T>);
  detail::tag_trait_helper<Tag>::debug_assert_true(
      base != nullptr, "continuation pointer is null");

  // (9) - this acquire-load synchronizes-with the release-CAS (2, 6)
  if (base->_next.load(std::memory_order_acquire) ==
      FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T)) {
    // short path
    std::invoke(std::forward<F>(f), base->cast_move());
    base->destroy();
    detail::tag_trait_helper<Tag>::release(base);
    return;
  }

#ifdef FUTURES_COUNT_ALLOC
  constexpr std::size_t lambda_size = sizeof(continuation_final<T, F, deleter_destroy>);
  constexpr double lambda_log2 = std::max(0.0, std::log2(lambda_size) - 2.0);
  std::size_t bucket =
      static_cast<std::size_t>(std::max(0., std::ceil(lambda_log2) - 1));

  FUTURES_INC_ALLOC_COUNTER(number_of_final_usage);
  FUTURES_INC_ALLOC_COUNTER(histogram_final_lambda_sizes[bucket]);
#endif

  // try to emplace the final into the steps local memory
  continuation<T>* cont;
  void* mem = base->template try_allocate<continuation_final<T, F, deleter_destroy>>();
  if (mem != nullptr) {
    cont = new (mem)
        continuation_final<T, F, deleter_destroy>(std::in_place, std::forward<F>(f));
    FUTURES_INC_ALLOC_COUNTER(number_of_prealloc_usage);
  } else {
    cont = detail::allocate_frame_noexcept<Tag, continuation_final<T, F, deleter_dealloc<Tag>>>(
        std::in_place, std::forward<F>(f));
  }

  detail::tag_trait_helper<Tag>::assert_true(
      cont != nullptr, "finally allocation failed unexpectedly");
  continuation<T>* expected = nullptr;
  // (10) - the release-store synchronizes-with the acquire-(re)load (1, 2, 5, 6)
  //        the acquire-reload synchronizes-with the release-CAS (2, 6)
  // Note: it would be sufficient to use memory_order_release for the success cases, but
  // since TSan does not support failure-orders, we use acq-rel to avoid false positives.
  if (!base->_next.compare_exchange_strong(expected, cont, std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    if (expected == FUTURES_INVALID_POINTER_PROMISE_ABANDONED(T)) {
      cont->operator()(detail::handler_helper<Tag, T>::abandon_promise(base->get_backtrace()));
    } else {
      detail::tag_trait_helper<Tag>::assert_true(expected == FUTURES_INVALID_POINTER_PROMISE_FULFILLED(T),
          "invalid continuation state");
      cont->operator()(base->cast_move());  // TODO we can get rid of this virtual call
      base->destroy();
    }

    detail::tag_trait_helper<Tag>::release(base);
  }
}

}

#endif
