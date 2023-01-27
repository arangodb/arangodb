////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "Actor/ActorBase.h"
#include "Actor/ActorList.h"

using namespace arangodb::pregel::actor;

struct ActorBaseMock : ActorBase {
  ActorBaseMock(std::string type) : type{std::move(type)} {};
  ActorBaseMock() = default;
  ~ActorBaseMock() = default;
  auto process(ActorPID sender, std::unique_ptr<MessagePayloadBase> payload)
      -> void override{};
  auto process(ActorPID sender, arangodb::velocypack::SharedSlice msg)
      -> void override{};
  auto typeName() -> std::string_view override { return type; };
  auto serialize() -> arangodb::velocypack::SharedSlice override {
    return arangodb::velocypack::SharedSlice();
  };
  auto finish() -> void override { finished = true; };
  auto finishedAndIdle() -> bool override { return finished; };

  std::string type;
  bool finished = false;
};

TEST(ActorListTest, finds_actor_by_actor_id_in_list) {
  auto list =
      ActorList({{{ActorID{1}, std::make_shared<ActorBaseMock>("some")},
                  {ActorID{2}, std::make_shared<ActorBaseMock>("search")},
                  {ActorID{3}, std::make_shared<ActorBaseMock>("some")},
                  {ActorID{4}, std::make_shared<ActorBaseMock>("some")}}});
  auto foundActor = list.find(ActorID{2});
  ASSERT_EQ(foundActor.value()->typeName(), "search");
}

TEST(ActorListTest, gives_nothing_when_searching_for_unknown_actor_id) {
  auto list = ActorList({{{ActorID{1}, std::make_shared<ActorBaseMock>()},
                          {ActorID{2}, std::make_shared<ActorBaseMock>()},
                          {ActorID{3}, std::make_shared<ActorBaseMock>()},
                          {ActorID{4}, std::make_shared<ActorBaseMock>()}}});
  auto foundActor = list.find(ActorID{10});
  ASSERT_EQ(foundActor, std::nullopt);
}

TEST(ActorListTest, adds_actor_to_list) {
  auto list = ActorList{};
  ASSERT_EQ(list.size(), 0);

  list.add(ActorID{1}, std::make_shared<ActorBaseMock>());
  ASSERT_EQ(list.size(), 1);
}

TEST(ActorListTest, neglects_added_actors_with_already_existing_actor_id) {
  auto list =
      ActorList({{{ActorID{1}, std::make_shared<ActorBaseMock>("existing")}}});

  list.add(ActorID{1}, std::make_shared<ActorBaseMock>("added"));
  ASSERT_EQ(list.size(), 1);
  ASSERT_EQ(list.find(ActorID{1}).value()->typeName(), "existing");
}

TEST(ActorListTest, removes_actor_by_id_from_list) {
  auto list = ActorList({{{ActorID{1}, std::make_shared<ActorBaseMock>()}}});
  ASSERT_EQ(list.size(), 1);

  list.remove(ActorID{1});
  ASSERT_EQ(list.size(), 0);
}

TEST(ActorListTest, removes_actor_in_use_without_destroying_it) {
  auto list = ActorList({{{ActorID{1}, std::make_shared<ActorBaseMock>()}}});
  ASSERT_EQ(list.size(), 1);

  auto actorInUse = list.find(ActorID{1});
  list.remove(ActorID{1});
  ASSERT_EQ(list.size(), 0);
  ASSERT_EQ(actorInUse->use_count(), 1);
}

TEST(ActorListTest, gives_all_actor_ids_in_list) {
  auto list = ActorList({{{ActorID{1}, std::make_shared<ActorBaseMock>()},
                          {ActorID{5}, std::make_shared<ActorBaseMock>()},
                          {ActorID{3}, std::make_shared<ActorBaseMock>()},
                          {ActorID{10}, std::make_shared<ActorBaseMock>()}}});
  auto ids = list.allIDs();
  ASSERT_EQ(ids.size(), 4);
  sort(ids.begin(), ids.end());
  ASSERT_EQ(ids, (std::vector<ActorID>{ActorID{1}, ActorID{3}, ActorID{5},
                                       ActorID{10}}));
}

TEST(ActorListTest, removes_actors_by_precondition_from_list) {
  auto list = ActorList(
      {{{ActorID{1}, std::make_shared<ActorBaseMock>("deletable")},
        {ActorID{2}, std::make_shared<ActorBaseMock>("non-deletable")},
        {ActorID{3}, std::make_shared<ActorBaseMock>("deletable")},
        {ActorID{4}, std::make_shared<ActorBaseMock>("deletable")}}});
  ASSERT_EQ(list.size(), 4);

  list.removeIf([](std::shared_ptr<ActorBase> const& actor) -> bool {
    return actor->typeName() == "deletable";
  });
  ASSERT_EQ(list.size(), 1);
  ASSERT_EQ(list.allIDs(), std::vector<ActorID>{ActorID{2}});
}

TEST(ActorListTest,
     removes_actors_by_precondition_without_destroying_actors_in_use) {
  auto list = ActorList(
      {{{ActorID{1}, std::make_shared<ActorBaseMock>("deletable")},
        {ActorID{2}, std::make_shared<ActorBaseMock>("non-deletable")},
        {ActorID{3}, std::make_shared<ActorBaseMock>("deletable")},
        {ActorID{4}, std::make_shared<ActorBaseMock>("deletable")}}});
  ASSERT_EQ(list.size(), 4);

  auto actorInUse = list.find(ActorID{1});
  list.removeIf([](std::shared_ptr<ActorBase> const& actor) -> bool {
    return actor->typeName() == "deletable";
  });
  ASSERT_EQ(list.size(), 1);
  ASSERT_EQ(list.allIDs(), std::vector<ActorID>{ActorID{2}});
  ASSERT_EQ(actorInUse->use_count(), 1);
}

TEST(ActorListTest, applies_function_to_each_actor) {
  auto list = ActorList({{{ActorID{1}, std::make_shared<ActorBaseMock>()},
                          {ActorID{2}, std::make_shared<ActorBaseMock>()},
                          {ActorID{3}, std::make_shared<ActorBaseMock>()},
                          {ActorID{4}, std::make_shared<ActorBaseMock>()}}});

  list.apply(
      [](std::shared_ptr<ActorBase>& actor) -> void { actor->finish(); });
  ASSERT_TRUE(list.find(ActorID{1}).value()->finishedAndIdle());
  ASSERT_TRUE(list.find(ActorID{2}).value()->finishedAndIdle());
  ASSERT_TRUE(list.find(ActorID{3}).value()->finishedAndIdle());
  ASSERT_TRUE(list.find(ActorID{4}).value()->finishedAndIdle());
}
