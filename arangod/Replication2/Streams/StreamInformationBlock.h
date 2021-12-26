////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <map>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/Streams/Streams.h"

namespace arangodb::replication2::streams {

template <typename Descriptor>
struct StreamInformationBlock;
template <StreamId Id, typename Type, typename Tags>
struct StreamInformationBlock<stream_descriptor<Id, Type, Tags>> {
  using StreamType = streams::Stream<Type>;
  using EntryType = StreamEntry<Type>;
  using Iterator = TypedLogRangeIterator<StreamEntryView<Type>>;

  using ContainerType =
      ::immer::flex_vector<EntryType, arangodb::immer::arango_memory_policy>;
  using TransientType = typename ContainerType::transient_type;
  using LogVariantType = std::variant<ContainerType, TransientType>;

  using WaitForResult = typename StreamType::WaitForResult;
  using WaitForPromise = futures::Promise<WaitForResult>;
  using WaitForQueue = std::multimap<LogIndex, WaitForPromise>;

  LogIndex _releaseIndex{0};
  LogVariantType _container;
  WaitForQueue _waitForQueue;

  auto appendEntry(LogIndex index, Type t);
  auto getWaitForResolveSet(LogIndex commitIndex) -> WaitForQueue;
  auto registerWaitFor(LogIndex index) -> futures::Future<WaitForResult>;
  auto getIterator() -> std::unique_ptr<Iterator>;
  auto getIteratorRange(LogIndex start, LogIndex stop) -> std::unique_ptr<Iterator>;

 private:
  auto getTransientContainer() -> TransientType&;
  auto getPersistentContainer() -> ContainerType&;
};

}  // namespace arangodb::replication2::streams
