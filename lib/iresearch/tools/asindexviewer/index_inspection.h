////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include <string>
#include <vector>
#include "velocypack/Builder.h"
#include "Inspection/VPackSaveInspector.h"

struct FieldHeader {
  std::string type;
  std::string logicalField;
  std::string analyzer;
  std::string storageNameBytes;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, FieldHeader& x) {
    return f.object(x).fields(f.field("type", x.type),
                              f.field("logicalField", x.logicalField),
                              f.field("analyzer", x.analyzer),
                              f.field("storageNameBytes", x.storageNameBytes));
  }
};

struct FieldTermMeta {
  uint64_t docsWithTerm;
  uint64_t freq;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, FieldTermMeta& x) {
    return f.object(x).fields(f.field("docsWithTerm", x.docsWithTerm),
                              f.field("freq", x.freq));
  }
};

struct TermPostings {
  std::string docId;
  uint64_t inDocFreq;
  std::vector<uint64_t> positions;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, TermPostings& x) {
    return f.object(x).fields(f.field("docId", x.docId),
                              f.field("inDocFreq", x.inDocFreq),
                              f.field("positions", x.positions));
  }
};

struct FieldTerms {

  std::string display;
  std::string rawBytes;
  FieldTermMeta termMeta;
  std::string payload;
  std::vector<TermPostings> postings;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, FieldTerms& x) {
    return f.object(x).fields(f.field("display", x.display),
                              f.field("rawBytes", x.rawBytes),
                              f.field("termMeta", x.termMeta),
                              f.field("payload", x.payload),
                              f.field("postings", x.postings));
  }
};

struct FieldIndexFeatures {

  std::string minTerm;
  std::string maxTerm;
  uint64_t termsCount;
  uint64_t docsCount;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, FieldIndexFeatures& x) {
    return f.object(x).fields(f.field("minTerm", x.minTerm),
                              f.field("maxTerm", x.minTerm),
                              f.field("termsCount", x.termsCount),
                              f.field("docsCount", x.docsCount));
  }
};

struct IndexedField {
  std::string name;
  FieldHeader fieldHeader;
  FieldIndexFeatures fieldIndexFeatures;
  std::vector<FieldTerms> terms;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, IndexedField& x) {
    return f.object(x).fields(f.field("name", x.name),
                              f.field("fieldIndexFeatures", x.fieldIndexFeatures),
                              f.field("terms", x.terms),
                              f.field("fieldHeader", x.fieldHeader));
  }
};

struct SegmentInfo {
  uint64_t segmentId;
  std::string name;
  uint64_t numDocs;
  uint64_t numLiveDocs;
  uint64_t byteSize;
  std::vector<IndexedField> fields;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, SegmentInfo& x) {
    return f.object(x).fields(f.field("segmentId", x.segmentId),
                              f.field("name", x.name),
                              f.field("numDocs", x.numDocs),
                              f.field("numLiveDocs", x.numLiveDocs),
                              f.field("byteSize", x.byteSize),
                              f.field("fields", x.fields));
  }
};

struct IndexInfo {
  uint64_t segmentsCount;
  uint64_t numDocs;
  uint64_t numLiveDocs;
  std::vector<SegmentInfo> segments;

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, IndexInfo& x) {
    return f.object(x).fields(f.field("numDocs", x.numDocs),
                              f.field("numLiveDocs", x.numLiveDocs),
                              f.field("segmentsCount", x.segmentsCount),
                              f.field("segments", x.segments));
  }
};

template<class T>
bool toVelocyPack(
    T& value, arangodb::velocypack::Builder& builder) {
  arangodb::inspection::VPackSaveInspector<> inspector{builder};
  if (auto status = inspector.apply(value); !status.ok()) {
    return false;
  }
  return true;
}
