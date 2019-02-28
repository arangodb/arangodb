////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewExecutor.h"

#include "IResearch/IResearchView.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::iresearch;

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    ExecutorInfos&& infos, std::shared_ptr<IResearchView::Snapshot const> reader,
    RegisterId const outputRegister)
    : ExecutorInfos(std::move(infos)),
      _outputRegister(outputRegister),
      _reader(std::move(reader)) {}

template <bool ordered>
IResearchViewExecutor<ordered>::IResearchViewExecutor(IResearchViewExecutor::Fetcher& fetcher,
                                                      IResearchViewExecutor::Infos&) {}

template <bool ordered>
std::pair<ExecutionState, typename IResearchViewExecutor<ordered>::Stats>
IResearchViewExecutor<ordered>::produceRow(OutputAqlItemRow& output) {
  return {ExecutionState::DONE, {}};
}

template class ::arangodb::aql::IResearchViewExecutor<false>;
