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
#pragma once

#include "Containers/Concurrent/snapshot.h"

#include <memory>
#include <vector>

namespace arangodb::containers {

template<typename List, typename F>
concept IteratorOverNodes = requires(List l, F f) {
  l.for_node(f);
};
template<typename List, typename Item, typename F>
concept IteratorOverSnapshots =
    IteratorOverNodes<List, F> && std::invocable<F, typename Item::Snapshot>;

template<typename List, HasSnapshot Item>
struct ListOfLists {
 private:
  std::vector<std::weak_ptr<List>> _lists;
  std::mutex _mutex;
  // std::shared_ptr<const Metrics> _metrics; // TODO

 public:
  // ListOfLists();
  auto add(std::shared_ptr<List> list) -> void {
    auto guard = std::lock_guard(_mutex);
    // make sure that expired nodes are deleted
    std::erase_if(_lists, [&](auto const& list) { return list.expired(); });
    _lists.emplace_back(list);
  }

  template<typename F>
  requires IteratorOverSnapshots<List, Item, F>
  auto for_node(F&& function) -> void {
    auto lists = [&] {
      auto guard = std::lock_guard(_mutex);
      return _lists;
    }();

    for (auto& weak_list : _lists) {
      if (auto list = weak_list.lock()) {
        list->for_node(function);
      }
    }
  }
  // auto set_metrics(std::shared_ptr<const Metrics> metrics) -> void; // TODO
  // void run_external_cleanup() noexcept;
};

}  // namespace arangodb::containers
