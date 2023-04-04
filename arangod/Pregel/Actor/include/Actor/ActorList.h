////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#include "Actor/ActorBase.h"
#include "Actor/ActorPID.h"
#include "Basics/Guarded.h"

namespace arangodb::pregel::actor {

template<typename F>
concept Predicate = requires(F fn, std::shared_ptr<ActorBase> const& actor) {
  { fn(actor) } -> std::same_as<bool>;
};

struct ActorList {
 private:
  struct ActorMap : std::unordered_map<ActorID, std::shared_ptr<ActorBase>> {};
  Guarded<ActorMap> actors;
  struct ActorInfo {
    ActorID id;
    std::string_view type;
  };

 public:
  ActorList(ActorMap map) : actors{std::move(map)} {}
  ActorList() = default;

  auto contains(ActorID id) const -> bool {
    return actors.doUnderLock(
        [id](ActorMap const& map) -> bool { return map.contains(id); });
  }

  auto find(ActorID id) const -> std::optional<std::shared_ptr<ActorBase>> {
    return actors.doUnderLock(
        [id](ActorMap const& map) -> std::optional<std::shared_ptr<ActorBase>> {
          auto a = map.find(id);
          if (a == std::end(map)) {
            return std::nullopt;
          }
          auto& [_, actor] = *a;
          return actor;
        });
  }

  auto add(ActorID id, std::shared_ptr<ActorBase> actor) -> void {
    actors.doUnderLock(
        [&id, &actor](ActorMap& map) { map.emplace(id, std::move(actor)); });
  }

  auto remove(ActorID id) -> void {
    actors.doUnderLock([id](ActorMap& map) { map.erase(id); });
  }

  template<Predicate F>
  auto removeIf(F&& isDeletable) -> void {
    actors.doUnderLock([&isDeletable](ActorMap& map) {
      std::erase_if(map, [&isDeletable](const auto& item) {
        auto& [_, actor] = item;
        return isDeletable(actor);
      });
    });
  }

  template<typename F>
  requires requires(F fn, std::shared_ptr<ActorBase>& actor) { {fn(actor)}; }
  auto apply(F&& fn) -> void {
    actors.doUnderLock([&fn](ActorMap& map) {
      for (auto&& [_, actor] : map) {
        fn(actor);
      }
    });
  }

  template<Predicate F>
  auto checkAll(F&& check) const -> bool {
    return actors.doUnderLock([&check](ActorMap const& map) {
      for (auto const& [_, actor] : map) {
        if (not check(actor)) {
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

  auto size() const -> size_t {
    return actors.doUnderLock([](ActorMap const& map) { return map.size(); });
  }

  template<typename Inspector>
  friend auto inspect(Inspector& f, ActorInfo& x);
  template<typename Inspector>
  friend auto inspect(Inspector& f, ActorMap const& x);
  template<typename Inspector>
  friend auto inspect(Inspector& f, ActorList const& x);
};
template<typename Inspector>
auto inspect(Inspector& f, ActorList::ActorInfo& x) {
  return f.object(x).fields(f.field("id", x.id), f.field("type", x.type));
}
template<typename Inspector>
auto inspect(Inspector& f, ActorList::ActorMap const& x) {
  if constexpr (Inspector::isLoading) {
    return inspection::Status{};
  } else {
    auto actorInfos = std::vector<ActorList::ActorInfo>{};
    actorInfos.reserve(x.size());
    for (auto const& [id, actor] : x) {
      actorInfos.emplace_back(
          ActorList::ActorInfo{.id = id, .type = actor->typeName()});
    }
    return f.apply(actorInfos);
  }
}
template<typename Inspector>
auto inspect(Inspector& f, ActorList const& x) {
  if constexpr (Inspector::isLoading) {
    return inspection::Status{};
  } else {
    return x.actors.doUnderLock(
        [&f](ActorList::ActorMap const& map) { return f.apply(map); });
  }
}

};  // namespace arangodb::pregel::actor
