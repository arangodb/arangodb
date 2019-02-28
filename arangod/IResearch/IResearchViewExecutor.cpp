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

#include "Aql/Query.h"
#include "IResearch/IResearchView.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::iresearch;

IResearchViewExecutorInfos::IResearchViewExecutorInfos(
    ExecutorInfos&& infos, std::shared_ptr<IResearchView::Snapshot const> reader,
    RegisterId const outputRegister, Query& query,
    iresearch::IResearchViewNode const& node)
    : ExecutorInfos(std::move(infos)),
      _outputRegister(outputRegister),
      _reader(std::move(reader)),
      _query(query),
      _node(node) {}

Query& IResearchViewExecutorInfos::getQuery() const noexcept {
  return _query;
}

IResearchViewNode const& IResearchViewExecutorInfos::getNode() const {
  return _node;
}

std::shared_ptr<const IResearchView::Snapshot> IResearchViewExecutorInfos::getReader() const {
  return _reader;
}

template <bool ordered>
IResearchViewExecutor<ordered>::IResearchViewExecutor(IResearchViewExecutor::Fetcher& fetcher,
                                                      IResearchViewExecutor::Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _filterCtx(1),  // arangodb::iresearch::ExpressionExecutionContext
      _ctx(&infos.getQuery(), infos.getNode()),
      _reader(infos.getReader()),
      _filter(irs::filter::prepared::empty()),
      _execCtx(*infos.getQuery().trx(), _ctx),
      _inflight(0),
      _hasMore(true),  // has more data initially
      _volatileSort(true),
      _volatileFilter(true) {}

template <bool ordered>
std::pair<ExecutionState, typename IResearchViewExecutor<ordered>::Stats>
IResearchViewExecutor<ordered>::produceRow(OutputAqlItemRow& output) {
  return {ExecutionState::DONE, {}};
}

template <bool ordered>
const IResearchViewExecutorInfos& IResearchViewExecutor<ordered>::infos() const noexcept {
  return _infos;
}

template class ::arangodb::aql::IResearchViewExecutor<false>;
