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

#ifndef ARANGOD_INDEXES_PATH_BASED_INDEX_H
#define ARANGOD_INDEXES_PATH_BASED_INDEX_H 1

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace aql {
enum AstNodeType : uint32_t;
}

class PathBasedIndex : public Index {
 protected:
  struct PermutationState {
    PermutationState(arangodb::aql::AstNodeType type,
                     arangodb::aql::AstNode const* value,
                     size_t attributePosition, size_t n)
        : type(type),
          value(value),
          attributePosition(attributePosition),
          current(0),
          n(n) {
      TRI_ASSERT(n > 0);
    }

    arangodb::aql::AstNode const* getValue() const;

    arangodb::aql::AstNodeType type;
    arangodb::aql::AstNode const* value;
    size_t const attributePosition;
    size_t current;
    size_t const n;
  };

 public:
  PathBasedIndex() = delete;

  PathBasedIndex(
      TRI_idx_iid_t, arangodb::LogicalCollection*,
      std::vector<std::vector<arangodb::basics::AttributeName>> const&,
      bool unique, bool sparse, bool allowPartialIndex);

  PathBasedIndex(TRI_idx_iid_t, arangodb::LogicalCollection*,
                 arangodb::velocypack::Slice const&, bool);

  PathBasedIndex(VPackSlice const&, bool);

  ~PathBasedIndex();

 public:

  /// @brief return the attribute paths
  std::vector<std::vector<std::string>> const& paths()
      const {
    return _paths;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the attribute paths, a -1 entry means none is expanding,
  /// otherwise the non-negative number is the index of the expanding one.
  //////////////////////////////////////////////////////////////////////////////

  std::vector<int> const& expanding() const {
    return _expanding;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the memory needed for an index key entry
  //////////////////////////////////////////////////////////////////////////////

  inline size_t elementSize() const {
    return TRI_index_element_t::memoryUsage(_paths.size());
  }

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper function to insert a document into any index type
  //////////////////////////////////////////////////////////////////////////////

  int fillElement(std::vector<TRI_index_element_t*>& elements,
                  TRI_doc_mptr_t const* document);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the number of paths
  //////////////////////////////////////////////////////////////////////////////

  inline size_t numPaths() const { return _paths.size(); }

 private:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper function to transform AttributeNames into string lists
  //////////////////////////////////////////////////////////////////////////////

  void fillPaths(std::vector<std::vector<std::string>>& paths,
                 std::vector<int>& expanding);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper function to create a set of index combinations to insert
  //////////////////////////////////////////////////////////////////////////////

  std::vector<VPackSlice> buildIndexValue(VPackSlice const documentSlice);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper function to create a set of index combinations to insert
  //////////////////////////////////////////////////////////////////////////////

  void buildIndexValues(VPackSlice const document, size_t level,
                        std::vector<std::vector<VPackSlice>>& toInsert,
                        std::vector<VPackSlice>& sliceStack);

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the attribute paths
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::vector<std::string>> _paths;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ... and which of them expands
  //////////////////////////////////////////////////////////////////////////////

  std::vector<int> _expanding;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not at least one attribute is expanded
  //////////////////////////////////////////////////////////////////////////////

  bool _useExpansion;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not partial indexing is allowed
  //////////////////////////////////////////////////////////////////////////////

  bool _allowPartialIndex;
};
}

#endif
