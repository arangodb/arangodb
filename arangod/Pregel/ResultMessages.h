////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <variant>
#include "Actor/ActorPID.h"
#include "Inspection/Types.h"
#include "Pregel/Worker/Messages.h"

namespace arangodb::pregel::message {

struct ResultStart {};
template<typename Inspector>
auto inspect(Inspector& f, ResultStart& x) {
  return f.object(x).fields();
}

struct OtherResultActorStarted {};
template<typename Inspector>
auto inspect(Inspector& f, OtherResultActorStarted& x) {
  return f.object(x).fields();
}

struct SaveResults {
  ResultT<PregelResults> results = {PregelResults{}};
};
template<typename Inspector>
auto inspect(Inspector& f, SaveResults& x) {
  return f.object(x).fields(f.field("results", x.results));
}

struct AddResults {
  ResultT<PregelResults> results = {PregelResults{}};
  bool receivedAllResults{false};
};
template<typename Inspector>
auto inspect(Inspector& f, AddResults& x) {
  return f.object(x).fields(
      f.field("results", x.results),
      f.field("receivedAllResults", x.receivedAllResults));
}

struct CleanupResultWhenExpired {};
template<typename Inspector>
auto inspect(Inspector& f, CleanupResultWhenExpired& x) {
  return f.object(x).fields();
}

struct CleanupResults {};
template<typename Inspector>
auto inspect(Inspector& f, CleanupResults& x) {
  return f.object(x).fields();
}

struct ResultMessages
    : std::variant<ResultStart, OtherResultActorStarted, SaveResults,
                   AddResults, CleanupResultWhenExpired, CleanupResults> {
  using std::variant<ResultStart, OtherResultActorStarted, SaveResults,
                     AddResults, CleanupResultWhenExpired,
                     CleanupResults>::variant;
};

template<typename Inspector>
auto inspect(Inspector& f, ResultMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<ResultStart>("Start"),
      arangodb::inspection::type<OtherResultActorStarted>(
          "OtherResultActorStarted"),
      arangodb::inspection::type<SaveResults>("SaveResults"),
      arangodb::inspection::type<AddResults>("AddResults"),
      arangodb::inspection::type<CleanupResultWhenExpired>("CleanupResult"),
      arangodb::inspection::type<CleanupResults>("CleanupResults"));
}

}  // namespace arangodb::pregel::message
