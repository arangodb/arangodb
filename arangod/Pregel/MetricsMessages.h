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
/// @author Aditya Mukhopadhyay
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <variant>
#include <cstdint>
#include "Inspection/Types.h"

namespace arangodb::pregel::metrics::message {
enum class PreviousState { LOADING, COMPUTING, STORING, OTHER };
template<typename Inspector>
auto inspect(Inspector& f, PreviousState& x) {
  return f.enumeration(x).values(
      PreviousState::LOADING, "LOADING", PreviousState::COMPUTING, "COMPUTING",
      PreviousState::STORING, "STORING", PreviousState::OTHER, "OTHER");
}

struct MetricsStart {};
template<typename Inspector>
auto inspect(Inspector& f, MetricsStart& x) {
  return f.object(x).fields();
}

struct ConductorStarted {};
template<typename Inspector>
auto inspect(Inspector& f, ConductorStarted& x) {
  return f.object(x).fields();
}

struct ConductorLoadingStarted {};
template<typename Inspector>
auto inspect(Inspector& f, ConductorLoadingStarted& x) {
  return f.object(x).fields();
}

struct ConductorComputingStarted {};
template<typename Inspector>
auto inspect(Inspector& f, ConductorComputingStarted& x) {
  return f.object(x).fields();
}

struct ConductorStoringStarted {};
template<typename Inspector>
auto inspect(Inspector& f, ConductorStoringStarted& x) {
  return f.object(x).fields();
}

struct ConductorFinished {
  PreviousState previousState = PreviousState::OTHER;
};
template<typename Inspector>
auto inspect(Inspector& f, ConductorFinished& x) {
  return f.object(x).fields(f.field("previousState", x.previousState));
}

struct WorkerStarted {};
template<typename Inspector>
auto inspect(Inspector& f, WorkerStarted& x) {
  return f.object(x).fields();
}

struct WorkerLoadingStarted {};
template<typename Inspector>
auto inspect(Inspector& f, WorkerLoadingStarted& x) {
  return f.object(x).fields();
}

struct WorkerLoadingFinished {
  uint64_t memoryConsumed;
};
template<typename Inspector>
auto inspect(Inspector& f, WorkerLoadingFinished& x) {
  return f.object(x).fields(f.field("memoryConsumed", x.memoryConsumed));
}

struct WorkerStoringStarted {};
template<typename Inspector>
auto inspect(Inspector& f, WorkerStoringStarted& x) {
  return f.object(x).fields();
}

struct WorkerStoringFinished {};
template<typename Inspector>
auto inspect(Inspector& f, WorkerStoringFinished& x) {
  return f.object(x).fields();
}

struct WorkerGssStarted {
  uint64_t threadsAdded;
};
template<typename Inspector>
auto inspect(Inspector& f, WorkerGssStarted& x) {
  return f.object(x).fields(f.field("threadsAdded", x.threadsAdded));
}

struct WorkerGssFinished {
  uint64_t threadsRemoved;
  uint64_t messagesSent;
  uint64_t messagesReceived;
};
template<typename Inspector>
auto inspect(Inspector& f, WorkerGssFinished& x) {
  return f.object(x).fields(f.field("threadsRemoved", x.threadsRemoved),
                            f.field("messagesSent", x.messagesSent),
                            f.field("messagesReceived", x.messagesReceived));
}

struct WorkerFinished {};
template<typename Inspector>
auto inspect(Inspector& f, WorkerFinished& x) {
  return f.object(x).fields();
}

using MetricsMessagesBase =
    std::variant<MetricsStart, ConductorStarted, ConductorLoadingStarted,
                 ConductorComputingStarted, ConductorStoringStarted,
                 ConductorFinished, WorkerStarted, WorkerLoadingStarted,
                 WorkerLoadingFinished, WorkerStoringStarted,
                 WorkerStoringFinished, WorkerGssStarted, WorkerGssFinished,
                 WorkerFinished>;
struct MetricsMessages : MetricsMessagesBase {
  using MetricsMessagesBase::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, MetricsMessages& x) {
  namespace insp = arangodb::inspection;

  return f.variant(x).unqualified().alternatives(
      insp::type<MetricsStart>("MetricsStart"),
      insp::type<ConductorStarted>("ConductorStarted"),
      insp::type<ConductorLoadingStarted>("ConductorLoadingStarted"),
      insp::type<ConductorComputingStarted>("ConductorComputingStarted"),
      insp::type<ConductorStoringStarted>("ConductorStoringStarted"),
      insp::type<ConductorFinished>("ConductorFinished"),
      insp::type<WorkerStarted>("WorkerStarted"),
      insp::type<WorkerLoadingStarted>("WorkerLoadingStarted"),
      insp::type<WorkerLoadingFinished>("WorkerLoadingFinished"),
      insp::type<WorkerStoringStarted>("WorkerStoringStarted"),
      insp::type<WorkerStoringFinished>("WorkerStoringFinished"),
      insp::type<WorkerGssStarted>("WorkerGssStarted"),
      insp::type<WorkerGssFinished>("WorkerGssFinished"),
      insp::type<WorkerFinished>("WorkerFinished"));
}
}  // namespace arangodb::pregel::metrics::message
