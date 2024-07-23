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
/// @author Markus Pfeiffer
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <condition_variable>
#include <initializer_list>
#include <memory>
#include <vector>
#include "Actor/ActorBase.h"
#include "Actor/ActorID.h"
#include "Basics/Guarded.h"

namespace arangodb::actor {

template<typename F, typename ActorPID>
concept Predicate =
    requires(F fn, std::shared_ptr<ActorBase<ActorPID>> const& actor) {
  { fn(actor) } -> std::same_as<bool>;
};

template<typename ActorPID>
struct ActorList {
  struct Entry {
    explicit Entry(std::shared_ptr<ActorBase<ActorPID>>&& actor)
        : actor(std::move(actor)) {}

    std::shared_ptr<ActorBase<ActorPID>> actor;
    std::vector<ActorID> monitors;

    template<class Inspector>
    friend inline auto inspect(Inspector& f, Entry& x) {
      return f.object(x).fields(f.field("type", x.actor->typeName()),
                                f.field("monitors", x.monitors));
    }
  };

  ActorList() = default;
  ActorList(std::initializer_list<
            std::pair<ActorID, std::shared_ptr<ActorBase<ActorPID>>>>
                list) {
    for (auto& [id, actor] : list) {
      add(id, std::move(actor));
    }
  }

  auto contains(ActorID id) const -> bool {
    return actors.doUnderLock(
        [id](ActorMap const& map) -> bool { return map.contains(id); });
  }

  auto find(ActorID id) const
      -> std::optional<std::shared_ptr<ActorBase<ActorPID>>> {
    return actors.doUnderLock(
        [id](ActorMap const& map)
            -> std::optional<std::shared_ptr<ActorBase<ActorPID>>> {
          auto a = map.find(id);
          if (a == std::end(map)) {
            return std::nullopt;
          }
          auto& [_, entry] = *a;
          return entry.actor;
        });
  }

  auto add(ActorID id, std::shared_ptr<ActorBase<ActorPID>> actor) -> void {
    actors.doUnderLock([&id, &actor](ActorMap& map) {
      map.emplace(id, Entry{std::move(actor)});
    });
  }

  auto remove(ActorID id) -> std::optional<Entry> {
    return actors.doUnderLock(
        [this, id](ActorMap& map) -> std::optional<Entry> {
          auto it = map.find(id);
          if (it != map.end()) {
            std::optional<Entry> result{std::move(it->second)};
            map.erase(it);
            if (map.empty()) {
              _finishBell.notify_all();
            }
            return result;
          }
          return {};
        });
  }

  template<typename F>
  requires requires(F fn, std::shared_ptr<ActorBase<ActorPID>>& actor) {
    {fn(actor)};
  }
  auto apply(F&& fn) -> void {
    actors.doUnderLock([&fn](ActorMap& map) {
      for (auto&& [_, entry] : map) {
        fn(entry.actor);
      }
    });
  }

  template<Predicate<ActorPID> F>
  auto checkAll(F&& check) const -> bool {
    return actors.doUnderLock([&check](ActorMap const& map) {
      for (auto const& [_, entry] : map) {
        if (not check(entry.actor)) {
          return false;
        }
      }
      return true;
    });
  }

  auto allIDs() const -> std::vector<ActorID> {
    return actors.doUnderLock([](ActorMap const& map) {
      auto res = std::vector<ActorID>{};
      res.reserve(map.size());
      for (auto const& [id, _] : map) {
        res.push_back(id);
      }
      return res;
    });
  }

  auto waitForAll() -> void {
    auto guard = actors.getLockedGuard();
    auto& map = guard.get();
    guard.wait(_finishBell, [&]() { return map.empty(); });
  }

  auto size() const -> size_t {
    return actors.doUnderLock([](ActorMap const& map) { return map.size(); });
  }

  auto monitor(ActorID monitoringActor, ActorID monitoredActor) -> bool {
    return actors.doUnderLock(
        [&monitoringActor, &monitoredActor](ActorMap& map) {
          auto it = map.find(monitoredActor);
          if (it == std::end(map)) {
            return false;
          }
          it->second.monitors.emplace_back(monitoringActor);
          return true;
        });
  }

  auto getMonitors(ActorID actor) -> std::vector<ActorID> {
    return actors.doUnderLock([&](auto& map) -> std::vector<ActorID> {
      auto it = map.find(actor);
      if (it != map.end()) {
        return it->second.monitors;
      }
      return {};
    });
  }

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, ActorList const& x) {
    static_assert(not Inspector::isLoading);
    return x.actors.doUnderLock(
        [&f](typename ActorList::ActorMap const& map) { return f.apply(map); });
  }

 private:
  struct ActorMap : std::unordered_map<ActorID, Entry> {};
  Guarded<ActorMap> actors;
  std::condition_variable _finishBell;
};

};  // namespace arangodb::actor
