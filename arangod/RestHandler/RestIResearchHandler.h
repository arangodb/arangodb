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

#pragma once

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "RestServer/ArangodServer.h"
#include "IResearch/IResearchDataStore.h"

namespace arangodb {

class IResearchFeature;

struct IResearchDatastoreSegmentInfo {
  std::string name;      //  segment name
  uint64_t numDocs;      //  no. of docs in the segment
  uint64_t numLiveDocs;  //  no. of live docs in the segment
  double deletionRatio;  //  deletion ratio = ((docs - liveDocs) / docs)
  uint64_t byteSize;     //  segment size in bytes

  template<class Inspector>
  friend inline auto inspect(Inspector& f, IResearchDatastoreSegmentInfo& x) {
    return f.object(x).fields(
        f.field("name", x.name), f.field("numDocs", x.numDocs),
        f.field("numLiveDocs", x.numLiveDocs), f.field("byteSize", x.byteSize),
        f.field("deletionRatio", x.deletionRatio));
  }
};

//  Structure to hold the stats of the entire
//  IResearch data store which may comprise of multiple
//  segments.
struct IResearchDatastoreStats {
  uint64_t numDocs;         //  total no. of docs in the store
  uint64_t numLiveDocs;     //  total no. of live docs
  double deletionRatio;     //  deletion ratio of the store
  uint64_t numPrimaryDocs;  //  total no. of primary docs
  uint64_t numSegments;     //  total no. of segments
  uint64_t numFiles;        //  total no. of files representing all segments
  uint64_t indexSize;       //  index size in bytes
  std::vector<IResearchDatastoreSegmentInfo> segments;  //  segments info

  template<class Inspector>
  friend inline auto inspect(Inspector& f, IResearchDatastoreStats& x) {
    return f.object(x).fields(
        f.field("numDocs", x.numDocs), f.field("numLiveDocs", x.numLiveDocs),
        f.field("deletionRatio", x.deletionRatio),
        f.field("numPrimaryDocs", x.numPrimaryDocs),
        f.field("numSegments", x.numSegments), f.field("numFiles", x.numFiles),
        f.field("indexSize", x.indexSize), f.field("segments", x.segments));
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief REST handler for accessing internal IResearch functionality
///
/// This handler provides access to internal IResearch operations and statistics
/// that are not available through the standard ArangoDB APIs.
///
/// Endpoints:
/// GET /_arango/experimental/_db/${db}/_admin/arangosearch/stats
/// Get IResearch feature statistics
///
/// TODO: COR-800
/// GET /_api/iresearch/threads - Get thread pool information
///
/// TODO: COR-801
/// GET /_api/iresearch/datastore/<collection>/<index> - Get data store stats
////////////////////////////////////////////////////////////////////////////////
class RestIResearchHandler : public RestVocbaseBaseHandler {
 public:
  RestIResearchHandler(application_features::ApplicationServer& server,
                       GeneralRequest* request, GeneralResponse* response);

  virtual RestStatus execute() override;

  virtual RequestLane lane() const override { return RequestLane::CLIENT_SLOW; }

  virtual char const* name() const override { return "RestIResearchHandler"; }

  template<typename T>
  static bool toVelocyPack(T& value, arangodb::velocypack::Builder& builder);

 protected:
  /// @brief Get data store statistics for a specific index
  bool getDatastoreStats(IResearchDatastoreStats& stats);

 private:
  std::shared_ptr<iresearch::IResearchDataStore> getIResearchDatastore();
};

}  // namespace arangodb
