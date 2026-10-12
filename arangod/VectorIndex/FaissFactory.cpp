////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2026 ArangoDB GmbH, Hyderabad, India
/// Copyright 2026 triAGENS GmbH, Hyderabad, India
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
/// Copyright holder is ArangoDB GmbH, Hyderabad, India
///
////////////////////////////////////////////////////////////////////////////////

#include "VectorIndex/FaissFactory.h"

#include "Basics/voc-errors.h"

#include <faiss/Index.h>
#include <faiss/IndexIVF.h>
#include <faiss/index_factory.h>

#include <cstddef>
#include <exception>
#include <format>
#include <memory>
#include <string>
#include <variant>

namespace arangodb::vector {

ResultT<std::shared_ptr<faiss::IndexIVF>> createIvfIndexFromFactory(
    UserDefinition const& def, std::size_t nLists) {
  TRI_ASSERT(def.factory.has_value());
  auto const factoryString = resolveFactoryString(*def.factory, nLists);

  std::shared_ptr<faiss::Index> index;
  try {
    index.reset(faiss::index_factory(static_cast<int>(def.dimension),
                                     factoryString.c_str(),
                                     metricToFaissMetric(def.metric)));
  } catch (std::exception const& e) {
    return Result{TRI_ERROR_BAD_PARAMETER,
                  std::format("Invalid factory string '{}': {}", factoryString,
                              e.what())};
  }

  auto ivfIndex = std::dynamic_pointer_cast<faiss::IndexIVF>(index);
  if (ivfIndex == nullptr) {
    return Result{
        TRI_ERROR_BAD_PARAMETER,
        std::format("Invalid factory string '{}': expected an IVF index",
                    factoryString)};
  }

  if (static_cast<std::size_t>(ivfIndex->nlist) != nLists) {
    return Result{
        TRI_ERROR_BAD_PARAMETER,
        std::format("The nLists parameter ({}) has to agree with the actual "
                    "nlists implied by the factory string '{}' (which is {})",
                    nLists, factoryString, ivfIndex->nlist)};
  }
  return ivfIndex;
}

Result validateFactoryString(UserDefinition const& def) {
  if (!def.factory) {
    return {};
  }

  // Try creating since we use faiss own parser for the factory string
  if (auto const* fixedNLists = std::get_if<std::size_t>(&def.nLists)) {
    return createIvfIndexFromFactory(def, *fixedNLists).result();
  }

  if (!isFactoryAStringScaling(*def.factory)) {
    return {TRI_ERROR_BAD_PARAMETER,
            std::format("The factory string '{}' fixes nLists, which conflicts "
                        "with a scaling nLists specification; give a fixed "
                        "nLists or use 'IVF{{}}'",
                        *def.factory)};
  }

  // Any concrete value works for checking that the template is parseable.
  auto const& spec = std::get<NListsScalingSpec>(def.nLists);
  return createIvfIndexFromFactory(def, spec.minNLists).result();
}

}  // namespace arangodb::vector
