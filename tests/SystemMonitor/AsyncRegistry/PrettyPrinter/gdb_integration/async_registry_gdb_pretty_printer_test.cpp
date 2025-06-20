////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include "Async/Registry/registry_variable.h"
#include "Containers/Concurrent/thread.h"

#include <csignal>
#include <iostream>
#include <source_location>

using namespace arangodb::async_registry;

auto breakpoint() { raise(SIGINT); }

auto format(arangodb::basics::SourceLocationSnapshot const& loc)
    -> std::string {
  return fmt::format("\"{}\" (\"{}\":{})", loc.function_name, loc.file_name,
                     loc.line);
}
auto format(arangodb::basics::ThreadId const& thread) -> std::string {
  return fmt::format("LWPID {} (pthread {})", thread.kernel_id,
                     thread.posix_id);
}
auto format(arangodb::basics::ThreadInfo const& thread) -> std::string {
  return fmt::format("\"{}\" (LWPID {})", thread.name, thread.kernel_id);
}
auto format(PromiseSnapshot const& snapshot) -> std::string {
  if (snapshot.thread == std::nullopt) {
    return fmt::format("{}, owned by {}, {}", format(snapshot.source_location),
                       format(snapshot.owning_thread),
                       arangodb::inspection::json(snapshot.state));
  } else {
    return fmt::format("{}, owned by {}, {} on {}",
                       format(snapshot.source_location),
                       format(snapshot.owning_thread),
                       arangodb::inspection::json(snapshot.state),
                       format(snapshot.thread.value()));
  }
}

/**
   This test creates a new registry and consecutively adds more promises to this
   registry. For each new promise added there exists a breakpoint where the
   corresponding gdb script will pause and compare the string representation of
   the registry against the given expected variable.

   We use here a completely new registry and not the global async registry to
   add parts of the registry by hand (which would otherwise not easily be
   possible) in order to test all possible scenarios.
 */
int main() {
  [[maybe_unused]] auto finished = false;

  // empty registry
  auto test_registry = Registry{};
  auto thread_registry = ThreadRegistry::make();
  auto current_thread = arangodb::basics::ThreadInfo::current();
  test_registry.add(thread_registry);

  breakpoint();

  auto expected = std::string("async registry");

  breakpoint();

  // add a promise
  auto parent = thread_registry->add([&]() {
    return Promise{CurrentRequester{current_thread},
                   std::source_location::current()};
  });
  expected = fmt::format(
      "async registry = {{\n"
      "[{}] = \n"
      "  ┌ {}\n"
      "─ {}}}",
      format(current_thread.get_ref().value()), format(parent->data.snapshot()),
      format(current_thread.get_ref().value()));

  breakpoint();

  // works also with a currently non-running promise
  parent->data.running_thread.store(std::nullopt);
  expected = fmt::format(
      "async registry = {{\n"
      "[{}] = \n"
      "  ┌ {}\n"
      "─ {}}}",
      format(current_thread.get_ref().value()), format(parent->data.snapshot()),
      format(current_thread.get_ref().value()));

  breakpoint();

  // add a promise that depends on parent promise
  auto* child = thread_registry->add([&]() {
    return Promise{{parent->data.id()}, std::source_location::current()};
  });
  expected = fmt::format(
      "async registry = {{\n"
      "[{}] = \n"
      "    ┌ {}\n"
      "  ┌ {}\n"
      "─ {}}}",
      format(current_thread.get_ref().value()), format(child->data.snapshot()),
      format(parent->data.snapshot()),
      format(current_thread.get_ref().value()));

  breakpoint();

  // add another promise that depends on parent promise
  auto* second_child = thread_registry->add([&]() {
    return Promise{{parent->data.id()}, std::source_location::current()};
  });
  expected = fmt::format(
      "async registry = {{\n"
      "[{}] = \n"
      "    ┌ {}\n"
      "    ├ {}\n"
      "  ┌ {}\n"
      "─ {}}}",
      format(current_thread.get_ref().value()), format(child->data.snapshot()),
      format(second_child->data.snapshot()), format(parent->data.snapshot()),
      format(current_thread.get_ref().value()));

  breakpoint();

  // add a child to a child promise
  auto* child_of_child = thread_registry->add([&]() {
    return Promise{{child->data.id()}, std::source_location::current()};
  });
  expected = fmt::format(
      "async registry = {{\n"
      "[{}] = \n"
      "      ┌ {}\n"
      "    ┌ {}\n"
      "    ├ {}\n"
      "  ┌ {}\n"
      "─ {}}}",
      format(current_thread.get_ref().value()),
      format(child_of_child->data.snapshot()), format(child->data.snapshot()),
      format(second_child->data.snapshot()), format(parent->data.snapshot()),
      format(current_thread.get_ref().value()));

  breakpoint();

  // add a child to the second child promise
  auto* child_of_second_child = thread_registry->add([&]() {
    return Promise{{second_child->data.id()}, std::source_location::current()};
  });
  expected = fmt::format(
      "async registry = {{\n"
      "[{}] = \n"
      "      ┌ {}\n"
      "    ┌ {}\n"
      "    │ ┌ {}\n"
      "    ├ {}\n"
      "  ┌ {}\n"
      "─ {}}}",
      format(current_thread.get_ref().value()),
      format(child_of_child->data.snapshot()), format(child->data.snapshot()),
      format(child_of_second_child->data.snapshot()),
      format(second_child->data.snapshot()), format(parent->data.snapshot()),
      format(current_thread.get_ref().value()));

  breakpoint();

  // add a completely unrelated promise
  auto* second_parent = thread_registry->add([&]() {
    return Promise{{arangodb::basics::ThreadInfo::current()},
                   std::source_location::current()};
  });
  expected = fmt::format(
      "async registry = {{\n"
      "[{}] = \n"
      "  ┌ {}\n"
      "─ {}, \n"
      "[{}] = \n"
      "      ┌ {}\n"
      "    ┌ {}\n"
      "    │ ┌ {}\n"
      "    ├ {}\n"
      "  ┌ {}\n"
      "─ {}}}",
      format(current_thread.get_ref().value()),
      format(second_parent->data.snapshot()),
      format(current_thread.get_ref().value()),
      format(current_thread.get_ref().value()),
      format(child_of_child->data.snapshot()), format(child->data.snapshot()),
      format(child_of_second_child->data.snapshot()),
      format(second_child->data.snapshot()), format(parent->data.snapshot()),
      format(current_thread.get_ref().value()));

  breakpoint();

  auto second_thread_registry = ThreadRegistry::make();
  auto other_thread =
      arangodb::basics::ThreadInfo{};  // simulate another thread
  test_registry.add(second_thread_registry);

  // add a new promise on another thread
  auto* parent_on_other_thread = second_thread_registry->add([&]() {
    return Promise{{other_thread}, std::source_location::current()};
  });
  expected = fmt::format(
      "async registry = {{\n"
      "[{}] = \n"
      "  ┌ {}\n"
      "─ {}, \n"
      "[{}] = \n"
      "      ┌ {}\n"
      "    ┌ {}\n"
      "    │ ┌ {}\n"
      "    ├ {}\n"
      "  ┌ {}\n"
      "─ {}, \n"
      "[{}] = \n"
      "  ┌ {}\n"
      "─ {}}}",
      format(current_thread.get_ref().value()),
      format(second_parent->data.snapshot()),
      format(current_thread.get_ref().value()),
      format(current_thread.get_ref().value()),
      format(child_of_child->data.snapshot()), format(child->data.snapshot()),
      format(child_of_second_child->data.snapshot()),
      format(second_child->data.snapshot()), format(parent->data.snapshot()),
      format(current_thread.get_ref().value()), format(other_thread),
      format(parent_on_other_thread->data.snapshot()), format(other_thread));

  breakpoint();

  finished = true;
  breakpoint();

  return 0;
}
