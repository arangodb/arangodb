#pragma once

#include <iostream>
#include <source_location>
#include <string>
#include <atomic>

namespace arangodb::coroutine {

enum class State {
  Initialized = 0,
  Transforming,
  Suspended,
  Resumed,
};

auto to_string(State state) -> std::string_view {
  switch (state) {
    case State::Initialized:
      return "initialized";
    case State::Transforming:
      return "transforming";
    case State::Suspended:
      return "suspended";
    case State::Resumed:
      return "resumed";
  }
  std::abort();
}

struct Observables {
  Observables(std::source_location loc) : _where(std::move(loc)) {}

  std::source_location _where;
  // TODO needs to be atomic, move this to PromiseInList
  State _state{};
};

std::ostream& operator<<(std::ostream& out, const Observables& observables) {
  return out << "[" << to_string(observables._state) << "] "
             << observables._where.function_name() << " : "
             << observables._where.file_name() << ":"
             << observables._where.line();
}

struct PromiseInList : Observables {
  PromiseInList(std::source_location loc) : Observables(std::move(loc)) {}
  std::atomic<PromiseInList*> next;
  // only needed to remove an item
  std::atomic<PromiseInList*> previous;
  // identifies the list it belongs to
  void* list_id;
};

std::ostream& operator<<(std::ostream& out, const PromiseInList& promise) {
  return out << static_cast<Observables>(promise);
}

}  // namespace arangodb::coroutine
