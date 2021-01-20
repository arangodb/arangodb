#ifndef MELLON_STATE_MACHINE_H
#define MELLON_STATE_MACHINE_H
#include <exception>
#include <functional>
#include <stdexcept>

template <auto state, typename F>
struct state_function_pair : F {
  explicit state_function_pair(F f) : F(std::move(f)) {}

  using function_type = F;
  static constexpr auto state_value = state;

  F& function_ref() { return *this; }
};

struct selector_not_found {};

template <auto state, typename... Ss>
struct state_function_selector;
template <typename E, E state, E other, typename F, typename... Ts>
struct state_function_selector<state, state_function_pair<other, F>, Ts...> {
  using type =
  std::conditional_t<state == other, state_function_pair<other, F>,
                     typename state_function_selector<state, Ts...>::type>;
};
template <typename E, E state>
struct state_function_selector<state> {
  using type = selector_not_found;
};

template <typename, typename, auto, auto, typename...>
struct state_machine;

template <typename E, typename T>
struct state_machine_result {
  E next_state;
  T result;  // TODO make void safe

  using result_type = T;
  using state_type = E;
};
template <typename T>
struct is_state_machine_result : std::false_type {};
template <typename E, typename T>
struct is_state_machine_result<state_machine_result<E, T>> : std::true_type {};
template <typename T>
inline constexpr auto is_state_machine_result_v =
    is_state_machine_result<T>::value;

template <auto Next, typename T>
auto continue_at(T&& t) {
  return state_machine_result<decltype(Next), std::decay_t<T>>{
      Next, std::forward<T>(t)};
}

void baz(int);

template <typename E, typename ResultType, E InitState, E FinalState,
    E... states, typename... Fs>
struct state_machine<E, ResultType, InitState, FinalState,
                     state_function_pair<states, Fs>...>
    : private state_function_pair<states, Fs>... {

// assert that the finale state is special
  static_assert(((FinalState != states) && ...));
  // assert that the init state is used
  static_assert(((InitState == states) || ...));
  // TODO add an assert, that the states are pairwise different

  template <typename... Gs>
  explicit state_machine(std::in_place_t, Gs&&... gs)
      : state_function_pair<states, Fs>(std::forward<Gs>(gs))... {}

  template <E state, typename... Ts>
  void run_state(Ts&&... ts) noexcept {
    if constexpr (state == FinalState) {
      // fulfill promise
      baz(std::forward<Ts>(ts)...);
    } else {
      using state_type = select_state<state>;
      using function_type = typename state_type::function_type;
      static_assert(std::is_nothrow_invocable_v<function_type, Ts...>);
      auto result = invoke_state<state>(std::forward<Ts>(ts)...);
      using result_type = decltype(result);
      static_assert(is_state_machine_result_v<result_type>);
      static_assert(std::is_same_v<typename result_type::state_type, E>);
      run_dyn_state(result.next_state, std::move(result.result));
    }
  }

  template <typename... Ts>
  void run_dyn_state(E next_state, Ts&&... ts) noexcept {
    run_dyn_state_collection(all_states{}, next_state, std::forward<Ts>(ts)...);
  }

  template <typename... Ts>
  void start(Ts&&... ts) {
    run_state<InitState>(std::forward<Ts>(ts)...);
  }

 private:
  template <E...>
  struct state_collection {};

  using all_states = state_collection<FinalState, states...>;

  template <E... Es, typename... Ts>
  void run_dyn_state_collection(state_collection<Es...>, E next_state,
      Ts&&... ts) noexcept {
    const bool was_found = ([&] {
      if (Es == next_state) {
        run_state<Es>(std::forward<Ts>(ts)...);
        return true;
      }
      return false;
    }() || ...);
    if (!was_found) {
      std::abort();
    }
  }

  template <E state, typename... Ts>
  auto invoke_state(Ts&&... ts) noexcept {
    using state_type = select_state<state>;
    using function_type = typename state_type::function_type;
    static_assert(std::is_nothrow_invocable_v<function_type, Ts...>);
    return std::invoke(
        state_function_pair<state, function_type>::function_ref(),
        std::forward<Ts>(ts)...);
  }

  template <E state>
  using select_state = typename state_function_selector<
      state, state_function_pair<states, Fs>...>::type;
};

void foo() {
  enum my_states { INIT, ADD_TWO, DONE, UNUSED };

  auto lambda1 = [](int x) mutable noexcept{ return continue_at<UNUSED>(15 * x); };

  auto lambda2 = [](int x) mutable noexcept {
    if (x > 100) {
      return continue_at<DONE>(x);
    }
    return continue_at<ADD_TWO>(x + 2);
  };

  state_machine<my_states, int, INIT, DONE,
                state_function_pair<INIT, decltype(lambda1)>,
                state_function_pair<ADD_TWO, decltype(lambda2)>>
      s(std::in_place, lambda1, lambda2);

  s.start(3);
};

#endif  //MELLON_STATE_MACHINE_H
