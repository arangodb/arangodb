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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "Quiver.h"
#include <cstdint>

#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/WCC/WCCValue.h"

template struct arangodb::pregel::Quiver<int64_t, int64_t>;
template struct arangodb::pregel::Quiver<uint64_t, uint64_t>;
template struct arangodb::pregel::Quiver<uint64_t, uint8_t>;
template struct arangodb::pregel::Quiver<float, float>;
template struct arangodb::pregel::Quiver<double, float>;
template struct arangodb::pregel::Quiver<double, double>;
template struct arangodb::pregel::Quiver<float, uint8_t>;

// specific algo combos
template struct arangodb::pregel::Quiver<arangodb::pregel::algos::WCCValue,
                                         uint64_t>;
template struct arangodb::pregel::Quiver<arangodb::pregel::algos::SCCValue,
                                         int8_t>;
template struct arangodb::pregel::Quiver<arangodb::pregel::algos::ECValue,
                                         int8_t>;
template struct arangodb::pregel::Quiver<arangodb::pregel::algos::HITSValue,
                                         int8_t>;
template struct arangodb::pregel::Quiver<
    arangodb::pregel::algos::HITSKleinbergValue, int8_t>;
template struct arangodb::pregel::Quiver<arangodb::pregel::algos::DMIDValue,
                                         float>;
template struct arangodb::pregel::Quiver<arangodb::pregel::algos::LPValue,
                                         int8_t>;
template struct arangodb::pregel::Quiver<arangodb::pregel::algos::SLPAValue,
                                         int8_t>;
template struct arangodb::pregel::Quiver<
    arangodb::pregel::algos::ColorPropagationValue, int8_t>;
