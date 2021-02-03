#ifndef FUTURES_COLLECTOR_H
#define FUTURES_COLLECTOR_H
#include "futures.h"

namespace mellon {
template <typename T, typename Tag>
struct collector {
  ~collector() {
    detail::tag_trait_helper<Tag>::assert_true(
        futures.empty(), "a collector was not finalized properly");
  }

  using result_type = std::vector<T>;

  template <typename F, typename... Args,
            std::enable_if_t<std::is_invocable_r_v<future<T, Tag>, F, Args...>, int> = 0>
  void push_result(F&& f, Args&&... args) noexcept(
      std::is_nothrow_invocable_r_v<future<T, Tag>, F, Args...>) {
    if (futures.size() == futures.capacity()) {
      futures.reserve(futures.size() * 2);
    }

    auto fut = std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    futures.emplace_back(std::move(fut));
  }

  auto into_future() && -> future<result_type, Tag> {
    auto&& [f, p] = make_promise<result_type, Tag>();
    auto ctx = std::make_shared<context>(std::move(p), futures.size());

    std::move(*this).for_all([ctx](std::size_t idx, T&& r) noexcept {
      ctx->fulfill(idx, std::move(r));
    });

    return std::move(f);
  }

  template <typename F, typename Func = std::decay_t<F>,
            std::enable_if_t<std::is_nothrow_invocable_r_v<void, Func, std::size_t, T>, int> = 0>
  void for_all(F f) && {
    static_assert(std::is_copy_constructible_v<Func>);
    static_assert(std::is_nothrow_move_constructible_v<Func>);
    for (size_t i = 0; i < futures.size(); i++) {
      std::move(futures[i]).finally([f = f, i](T&& v) noexcept {
        std::invoke(f, i, std::move(v));
      });
    }
    futures.clear();
  }

  [[nodiscard]] auto empty() const noexcept -> bool { return futures.empty(); }

 private:
  struct context : std::enable_shared_from_this<context> {
    explicit context(promise<result_type, Tag>&& p, std::size_t size)
        : out_promise(std::move(p)), results(size, detail::box<T>{}) {}
    promise<result_type, Tag> out_promise;
    std::vector<detail::box<T>> results;

    ~context() {
      result_type v;
      v.reserve(results.size());

      for (auto&& r : results) {
        static_assert(std::is_nothrow_move_constructible_v<T>);
        v.emplace_back(r.cast_move());
        r.destroy();
      }
      std::move(out_promise).fulfill(std::move(v));
    }

    void fulfill(std::size_t idx, T&& v) noexcept {
      results[idx].emplace(std::move(v));
    }
  };

  std::vector<future<T, Tag>> futures;
};

template <typename C, typename F>
struct collect_guard : F {
  collect_guard(C& c, F f) : F(f), c(c) {}
  ~collect_guard() {
    if (!c.empty()) {
      std::move(c).for_all(function_self());
    }
  }

 private:
  F& function_self() { return *this; }

  C& c;
};

}  // namespace mellon

#endif  // FUTURES_COLLECTOR_H
