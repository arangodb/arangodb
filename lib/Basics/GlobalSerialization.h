////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

// This functionality should only ever be used in test code and the
// global synchronization points must only be in TRI_IF_FAILURE blocks.
// Otherwise this could have catastrophic consequences for performance.
// In particular should these synchronization primitimes never be used
// to ensure proper logic!

#include <string_view>

namespace arangodb {

// The following two functions can be used to serialize events globally
// in a cluster. Note that this only works for "local" clusters, in which
// all `arangod` instances share a common file system and use the same
// temporary path. This is the typical situation for our integration tests.
//
// In such a test, one can specify a straight line program of events,
// which provides the serialization which the test wants to stage.
// Each line of the straight line program is of the form
//
//   <SOURCEID> <SELECTOR> <LABEL>
//
// The line must contain exactly 3 spaces separating the 3 parts. The
// `<SOURCEID>` is an identifier used in the code to mark the code
// place, it corresponds to the `id` argument in the functions below.
// The `<SELECTOR>` is a string (without spaces) which can be used that
// not every passing of the code place triggers. It can for example be
// used to only trigger on a specific server or for a specific shard.
// This corresponds to the `selector` argument in the functions below.
// The `<LABEL>` part is only a label to indicate in logs, which line of
// the straight line program was triggered.
//
// The serialization semantics are implemented as follows. The straight
// line program resides in some file in the file system, to which all
// `arangod` instances have access. It is called `globalSLP` and resides
// in the temporary path (which can be specified by an environment variable).
// In the same directory there is a file `globalSLP_PC`, which serves
// as the program counter of the straight line program. This file is initially
// empty and each line of the straight line program which is triggered,
// is then appended to the file to track progress. In this way, there is
// always a "next line" to be executed, which is the first line in
// `globalSLP` which is not (yet) in `globalSLP_PC`.
//
// The semantics are as follows:
//
// If `waitForGlobalEvent` is called, then the SLP is read and it is
// determined, if the current line matches `id` and `selector`. If not,
// the function waits until it is. Once the current line matches the
// current line is advanced by one, a log message is written and the
// function returns. If the straight line program is completed (all
// lines were triggered) or the `globalSLP_PC` file no longer exists,
// then `waitForGlobalEvent` immediately returns with no further
// action.
//
// If `observeGlobalEvent` is called, then the SLP is read and it is
// determined, if the current line matches `id` and `selector`. If so,
// the current line is advanced, otherwise it stays. In any case, the
// function returns immediately.
//
// For an example test which uses this see
//   tests/js/client/shell/shell-move-shard-sync-fail-cluster.js
//
// PLEASE NOTE THAT THIS APPROACH HAS SOME SERIOUS LIMITATIONS:
//
//  - this only works in **local** clusters, in which all arangod instances
//    share the same file system and temporary path!
//  - the SLP file is **global**, it resides in a path which contains
//    the name of the test suite (e.g. shell-client), but should at some
//    stage multiple tests in this suite run concurrently **in the same
//    cluster**, this will break! Beware of this!

void waitForGlobalEvent(std::string_view id, std::string_view selector);
void observeGlobalEvent(std::string_view id, std::string_view selector);
}  // namespace arangodb
