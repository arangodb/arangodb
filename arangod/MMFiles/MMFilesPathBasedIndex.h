////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_PATH_BASED_INDEX_H
#define ARANGOD_MMFILES_PATH_BASED_INDEX_H 1

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace aql {
enum AstNodeType : uint32_t;
}

class FixedSizeAllocator;

class MMFilesPathBasedIndex : public Index {

 public:
  MMFilesPathBasedIndex() = delete;

  MMFilesPathBasedIndex(TRI_idx_iid_t, arangodb::LogicalCollection*,
                 arangodb::velocypack::Slice const&, size_t baseSize, bool allowPartialIndex);

  ~MMFilesPathBasedIndex();

 public:
  void toVelocyPackFigures(arangodb::velocypack::Builder&) const override;

  void toVelocyPack(arangodb::velocypack::Builder& builder,
                    bool withFigures,
                    bool forPersistence) const override;

  /// @brief return the attribute paths
  std::vector<std::vector<std::string>> const& paths()
      const {
    return _paths;
  }

  /// @brief return the attribute paths, a -1 entry means none is expanding,
  /// otherwise the non-negative number is the index of the expanding one.
  std::vector<int> const& expanding() const {
    return _expanding;
  }

  bool implicitlyUnique() const override;
  void load() override {}

 protected:
  /// @brief helper function to insert a document into any index type
  template<typename T>
  int fillElement(std::vector<T*>& elements, 
          TRI_voc_rid_t revisionId, arangodb::velocypack::Slice const&);

  /// @brief return the number of paths
  inline size_t numPaths() const { return _paths.size(); }

 private:

  /// @brief helper function to transform AttributeNames into string lists
  void fillPaths(std::vector<std::vector<std::string>>& paths,
                 std::vector<int>& expanding);

  /// @brief helper function to create a set of index combinations to insert
  std::vector<std::pair<VPackSlice, uint32_t>> buildIndexValue(VPackSlice const documentSlice);

  /// @brief helper function to create a set of index combinations to insert
  void buildIndexValues(VPackSlice const document, size_t level,
                        std::vector<std::vector<std::pair<VPackSlice, uint32_t>>>& toInsert,
                        std::vector<std::pair<VPackSlice, uint32_t>>& sliceStack);

 protected:
  std::unique_ptr<FixedSizeAllocator> _allocator;

  /// @brief the attribute paths
  std::vector<std::vector<std::string>> _paths;

  /// @brief ... and which of them expands
  std::vector<int> _expanding;

  /// @brief whether or not array indexes will de-duplicate their input values
  bool _deduplicate;

  /// @brief whether or not at least one attribute is expanded
  bool _useExpansion;

  /// @brief whether or not partial indexing is allowed
  bool _allowPartialIndex;
};
}

#endif
