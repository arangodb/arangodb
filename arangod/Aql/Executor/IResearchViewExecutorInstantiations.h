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
/// @author Tobias Gödderz
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#define INSTANTIATE_VIEW_EXECUTOR(T, MT)              \
  template class ExecutionBlockImpl<                  \
      T<ExecutionTraits<false, false, false, (MT)>>>; \
  template class ExecutionBlockImpl<                  \
      T<ExecutionTraits<false, false, true, (MT)>>>;  \
  template class ExecutionBlockImpl<                  \
      T<ExecutionTraits<false, true, false, (MT)>>>;  \
  template class ExecutionBlockImpl<                  \
      T<ExecutionTraits<false, true, true, (MT)>>>;   \
  template class ExecutionBlockImpl<                  \
      T<ExecutionTraits<true, false, false, (MT)>>>;  \
  template class ExecutionBlockImpl<                  \
      T<ExecutionTraits<true, false, true, (MT)>>>;   \
  template class ExecutionBlockImpl<                  \
      T<ExecutionTraits<true, true, false, (MT)>>>;   \
  template class ExecutionBlockImpl<T<ExecutionTraits<true, true, true, (MT)>>>;
