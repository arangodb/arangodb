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
/// @author Koushal Kawade
////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include "RestIResearchHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchDataStore.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/IResearchRocksDBInvertedIndex.h"
#include "Logger/LogMacros.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "VocBase/Methods/Collections.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/IndexesSnapshot.h"
#include "IResearch/IResearchDataStore.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {

RestIResearchHandler::RestIResearchHandler(
    application_features::ApplicationServer& server, GeneralRequest* request,
    GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

std::shared_ptr<iresearch::IResearchDataStore>
RestIResearchHandler::getIResearchDatastore() {
  std::shared_ptr<iresearch::IResearchDataStore> datastorePtr;

  //  Callback to process each enumerated collection
  auto processCollection =
      [&](std::shared_ptr<LogicalCollection> const& coll) -> bool {
    const IndexesSnapshot& idxSnapshot =
        coll->getPhysical()->getIndexesSnapshot();
    auto idxs = idxSnapshot.getIndexes();

    //  Find an inverted index or a view.
    //  It will lead us to IResearchDataStore.
    for (size_t i = 0; i < idxs.size(); i++) {
      const auto& idx = idxs[i];

      if (!idx || (Index::IndexType::IResearchLink != idx->type() &&
                   Index::IndexType::Inverted != idx->type())) {
        continue;
      }

      // TODO(MBkkt) find a better way to retrieve an IResearchDataStore
      //  cannot use downCast since Index is not related to IResearchDataStore
      datastorePtr =
          std::dynamic_pointer_cast<iresearch::IResearchDataStore>(idx);
      break;
    }

    return !datastorePtr;
  };

  methods::Collections::enumerate(&_vocbase, processCollection);
  return datastorePtr;
}

RestStatus RestIResearchHandler::execute() {
  if (!_request) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return RestStatus::DONE;
  }

  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  if (!ServerState::instance()->isSingleServer()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_CLUSTER_UNSUPPORTED);
    return RestStatus::DONE;
  }

  try {
    IResearchDatastoreStats result;
    if (!getDatastoreStats(result)) {
      generateResult(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
      return RestStatus::DONE;
    }

    VPackBuilder builder;
    if (!toVelocyPack(result, builder)) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "failed to serialize ArangoSearch stats");
      return RestStatus::DONE;
    }
    generateResult(rest::ResponseCode::OK, builder.slice());

  } catch (std::exception const& ex) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  ex.what());
  }

  return RestStatus::DONE;
}

bool RestIResearchHandler::getDatastoreStats(IResearchDatastoreStats& result) {
  auto dataStore = getIResearchDatastore();
  if (!dataStore) {
    //  Looks like no inverted index or views exist
    return false;
  }

  auto stats = dataStore->getDatastoreStats();
  auto& summary = stats.summary;

  result.numDocs = summary.numDocs;
  result.numLiveDocs = summary.numLiveDocs;

  if (summary.numDocs > 0) {
    auto deletionRatio =
        static_cast<double>(summary.numDocs - summary.numLiveDocs) /
        summary.numDocs;
    result.deletionRatio = std::round(deletionRatio * 100.0) / 100.0;
  } else {
    result.deletionRatio = 0.0;
  }

  result.numPrimaryDocs = summary.numPrimaryDocs;
  result.numSegments = summary.numSegments;
  result.numFiles = summary.numFiles;
  result.indexSize = summary.indexSize;

  auto& rSegments = result.segments;
  const auto& segments = stats.segments;
  for (const auto& segment : segments) {
    IResearchDatastoreSegmentInfo rSegment;
    rSegment.numDocs = segment.docs_count;
    rSegment.numLiveDocs = segment.live_docs_count;

    if (segment.docs_count > 0) {
      auto deletionRatio =
          static_cast<double>(segment.docs_count - segment.live_docs_count) /
          segment.docs_count;
      rSegment.deletionRatio = std::round(deletionRatio * 100.0) / 100.0;
    } else {
      rSegment.deletionRatio = 0.0;
    }

    rSegment.name = segment.name;
    rSegment.byteSize = segment.byte_size;

    rSegments.push_back(rSegment);
  }

  return true;
}

template<class T>
bool RestIResearchHandler::toVelocyPack(
    T& value, arangodb::velocypack::Builder& builder) {
  arangodb::inspection::VPackSaveInspector<> inspector{builder};
  if (auto status = inspector.apply(value); !status.ok()) {
    return false;
  }
  return true;
}

}  // namespace arangodb
