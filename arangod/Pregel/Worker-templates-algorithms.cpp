////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/Algos/AIR/AIR.h"

#include "Pregel/Worker.cpp"

// custom algorithm types
template class arangodb::pregel::Worker<uint64_t, uint64_t, SenderMessage<uint64_t>>;
template class arangodb::pregel::Worker<SCCValue, int8_t, SenderMessage<uint64_t>>;
template class arangodb::pregel::Worker<HITSValue, int8_t, SenderMessage<double>>;
template class arangodb::pregel::Worker<ECValue, int8_t, HLLCounter>;
template class arangodb::pregel::Worker<DMIDValue, float, DMIDMessage>;
template class arangodb::pregel::Worker<LPValue, int8_t, uint64_t>;
template class arangodb::pregel::Worker<SLPAValue, int8_t, uint64_t>;



using namespace arangodb::pregel::algos::accumulators;
template class arangodb::pregel::Worker<VertexData, EdgeData, MessageData>;

