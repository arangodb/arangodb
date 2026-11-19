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

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "index/directory_reader.hpp"
#include "index/index_reader.hpp"
#include "index/index_features.hpp"
#include "store/fs_directory.hpp"
#include "analysis/token_attributes.hpp"
#include "formats/formats_10_attributes.hpp"
#include "utils/string.hpp"
#include "utils/type_limits.hpp"
#include "utils/numeric_utils.hpp"  // Add this include at the top
#include "Basics/ScopeGuard.h"
#include "index_inspection.h"
using std::cout, std::endl;

// Mirrors arangodb::iresearch::kludge::kAnalyzerDelimiter and ArangoDB
// field-name suffixes (IResearchKludge.cpp).
inline constexpr char kArangoAnalyzerDelimiter = '\1';

enum class ArangoFieldKind {
  Unknown,
  Numeric,
  StringMangled,
  NullMangled,
  BoolMangled,
  AnalyzedString,
};

struct ArangoFieldTypeInfo {
  ArangoFieldKind kind{ArangoFieldKind::Unknown};
  std::string_view logical_name{};
  std::string_view type_suffix{};
};

// Same delimiter rule as IResearchKludge::demangleType: walk backward from the
// end and stop at the last byte with value <= '\1' (NUL or analyzer marker).
[[nodiscard]] ArangoFieldTypeInfo detectArangoFieldType(
    std::string_view const storage_name) noexcept {
  ArangoFieldTypeInfo out{.logical_name = storage_name, .type_suffix = {}};
  if (storage_name.empty()) {
    return out;
  }
  size_t i = storage_name.size() - 1;
  for (;;) {
    if (static_cast<unsigned char>(storage_name[i]) <=
        static_cast<unsigned char>(kArangoAnalyzerDelimiter)) {
      out.logical_name = storage_name.substr(0, i);
      out.type_suffix = storage_name.substr(i);
      std::string_view const tail = out.type_suffix;
      if (tail.size() >= 3 && tail[0] == '\0' && tail[1] == '_') {
        switch (tail[2]) {
          case 'd':
            out.kind = ArangoFieldKind::Numeric;
            return out;
          case 's':
            out.kind = ArangoFieldKind::StringMangled;
            return out;
          case 'n':
            out.kind = ArangoFieldKind::NullMangled;
            return out;
          case 'b':
            out.kind = ArangoFieldKind::BoolMangled;
            return out;
          default:
            break;
        }
      }
      if (tail.size() >= 2 && tail[0] == kArangoAnalyzerDelimiter) {
        out.kind = ArangoFieldKind::AnalyzedString;
        return out;
      }
      out.kind = ArangoFieldKind::Unknown;
      return out;
    }
    if (i == 0) {
      break;
    }
    --i;
  }
  out.kind = ArangoFieldKind::Unknown;
  return out;
}

[[nodiscard]] char const* arangoFieldKindLabel(ArangoFieldKind const k) noexcept {
  switch (k) {
    case ArangoFieldKind::Numeric:
      return "numeric";
    case ArangoFieldKind::StringMangled:
      return "string";
    case ArangoFieldKind::NullMangled:
      return "null";
    case ArangoFieldKind::BoolMangled:
      return "bool";
    case ArangoFieldKind::AnalyzedString:
      return "analyzed_string";
    case ArangoFieldKind::Unknown:
      return "unknown";
  }
  return "unknown";
}

[[nodiscard]] std::string hexBytes(irs::bytes_view const value,
                                   size_t const max_bytes = 64) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  size_t const n = std::min(value.size(), max_bytes);
  for (size_t i = 0; i < n; ++i) {
    if (i) {
      oss << ' ';
    }
    oss << std::setw(2) << static_cast<unsigned>(value[i]);
  }
  if (value.size() > max_bytes) {
    oss << " ... (" << std::dec << value.size() << " bytes)";
  }
  return oss.str();
}

[[nodiscard]] std::string escapeLogicalFieldName(std::string_view const s) {
  std::ostringstream oss;
  for (unsigned char const c : s) {
    if (c == '\n') {
      oss << "\\n";
    } else if (c == '\r') {
      oss << "\\r";
    } else if (c == '\t') {
      oss << "\\t";
    } else if (c == '\\') {
      oss << "\\\\";
    } else if (c >= 32 && c < 127) {
      oss << static_cast<char>(c);
    } else {
      oss << "\\x" << std::hex << std::setw(2) << std::setfill('0')
          << static_cast<unsigned>(c) << std::dec << std::setfill('0');
    }
  }
  return oss.str();
}

void printArangoFieldHeader(std::string_view const storage_name,
                            ArangoFieldTypeInfo const& t,
                            FieldHeader& bField) {

  bField.type = arangoFieldKindLabel(t.kind);
  bField.logicalField = escapeLogicalFieldName(t.logical_name);
  if (t.kind == ArangoFieldKind::AnalyzedString && t.type_suffix.size() > 1) {
    bField.analyzer = escapeLogicalFieldName(t.type_suffix.substr(1));
  }
  auto val = hexBytes(
              irs::bytes_view(reinterpret_cast<irs::byte_type const*>(
                                  storage_name.data()),
                              storage_name.size()));
  bField.storageNameBytes = val;
}

// Decode bytes produced by irs::numeric_token_stream (Arango numeric fields).
std::string printNumericTermValue(irs::bytes_view value) {
    if (value.empty()) {
        return "(empty)";
    }

    std::ostringstream oss;
    // Get the first byte which contains TYPE_MAGIC + shift
    irs::byte_type first_byte = value[0];

    // Check type ranges based on TYPE_MAGIC values:
    // int32:  0x00 + shift (shift can be 0-31, so range 0x00-0x1F)
    // float:  0x20 + shift (shift can be 0-31, so range 0x20-0x3F)  
    // int64:  0x60 + shift (shift can be 0-63, so range 0x60-0x7F)
    // double: 0xA0 + shift (shift can be 0-63, so range 0xA0-0xBF)

    oss << "(" << std::hex << first_byte << "): ";

    if (first_byte < 0x20) {
        // int32_t
        auto decoded = irs::numeric_utils::numeric_traits<int32_t>::decode(value.data());
        oss << "(int32: " << decoded << ")";
    } else if (first_byte >= 0x20 && first_byte < 0x40) {
        // float
        auto decoded = irs::numeric_utils::numeric_traits<float>::decode(value.data());
        oss << "(float: " << decoded << ")";
    } else if (first_byte >= 0x60 && first_byte < 0x80) {
        // int64_t
        auto decoded = irs::numeric_utils::numeric_traits<int64_t>::decode(value.data());
        oss << "(int64: " << decoded << ")";
    } else if (first_byte >= 0xA0 && first_byte <= 0xD0) {
        // double
        auto decoded = irs::numeric_utils::numeric_traits<double>::decode(value.data());
        oss << "(double: " << decoded << ")";
    } else {
        // Assume it's a string if it doesn't match any numeric type magic
        oss << "(string: " << irs::ViewCast<char>(value) << ")";
    }

    return oss.str();
}

[[nodiscard]] std::string formatTermForField(ArangoFieldKind const kind,
                                             irs::bytes_view const value) {
  switch (kind) {
    case ArangoFieldKind::Numeric:
      return printNumericTermValue(value);
    case ArangoFieldKind::BoolMangled:
      if (value.size() == 1) {
        if (value[0] == 0) {
          return "bool:false";
        }
        if (value[0] == static_cast<irs::byte_type>(0xFF)) {
          return "bool:true";
        }
      }
      return std::string("bool:raw ") + hexBytes(value);
    case ArangoFieldKind::NullMangled:
      if (value.empty()) {
        return "null";
      }
      return std::string("null:unexpected ") + hexBytes(value);
    case ArangoFieldKind::StringMangled:
    case ArangoFieldKind::AnalyzedString:
      return std::string("text:") + std::string(irs::ViewCast<char>(value));
    case ArangoFieldKind::Unknown:
      break;
  }
  // Non-Arango or unrecognized field: show UTF-8 view plus hex to avoid
  // mis-reading arbitrary bytes as numeric_token_stream encodings.
  std::ostringstream oss;
  oss << "rawUtf8=\"" << irs::ViewCast<char>(value) << "\" hex=" << hexBytes(value);
  return oss.str();
}

[[nodiscard]] std::string formatPostingPtr(uint64_t const v) {
  if (!irs::address_limits::valid(v)) {
    return "invalid";
  }
  std::ostringstream oss;
  oss << v;
  return oss.str();
}

// Stored by ArangoSearch per IResearchCommon / IResearchDataStore (column @_PK).
inline constexpr std::string_view kArangoPkColumn{"@_PK"};

using DocPkMap = std::unordered_map<irs::doc_id_t, std::string>;

[[nodiscard]] std::string formatPkColumnPayload(irs::bytes_view const v) {
  std::ostringstream oss;
  oss << "LocalDocumentId " << hexBytes(v);
  if (v.size() == sizeof(uint64_t)) {
    uint64_t le = 0;
    std::memcpy(&le, v.data(), sizeof(le));
    oss << " u64_le=" << le;
  }
  return oss.str();
}

// Map IResearch segment-local doc id -> formatted bytes from @_PK column.
[[nodiscard]] DocPkMap loadArangoPkByIresearchDoc(irs::SubReader const& segment) {
  DocPkMap out;
  irs::column_reader const* col = segment.column(kArangoPkColumn);
  if (col == nullptr) {
    return out;
  }
  // Match column-store dumping in this file (kConsolidation); kNormal can
  // skip rows for some columnstore layouts.
  auto it = col->iterator(irs::ColumnHint::kConsolidation);
  auto* pay = irs::get<irs::payload>(*it);
  if (pay == nullptr) {
    return out;
  }
  while (it->next()) {
    out[it->value()] = formatPkColumnPayload(pay->value);
  }
  return out;
}

// IResearch doc ids are 1..live_docs per segment; repeat in every segment.
// Prefix with segment index so "S0/d1" and "S2/d1" are visibly different rows.
std::string printSegmentLocalDocId(size_t const segment_index,
                            irs::doc_id_t const doc_id,
                            DocPkMap const* pk_by_doc) noexcept {
  std::ostringstream oss;
  oss << "S" << segment_index << "/d" << doc_id << " ";
  if (pk_by_doc != nullptr) {
    auto const i = pk_by_doc->find(doc_id);
    if (i != pk_by_doc->end()) {
      oss << "{" << i->second << "}";
    }
  }

  return oss.str();
}

// After next()/read(), prints attributes on the term iterator. See IResearch
// `term_meta`, `version10::term_meta` (formats_10_attributes.hpp), and
// burst-trie `term_iterator_base`.
void printTermLine(irs::term_iterator const& term,
                   ArangoFieldKind const field_kind,
                  FieldTerms& fieldTerm) {

  irs::bytes_view const v = term.value();
  fieldTerm.display = formatTermForField(field_kind, v);
  fieldTerm.rawBytes = hexBytes(v);

  //  Term meta
  auto& fieldTermMeta = fieldTerm.termMeta;
  if (auto const* tm = irs::get<irs::term_meta>(term)) {
    fieldTermMeta.docsWithTerm = tm->docs_count;
    //  freq = sum of within-doc occurrences across all postings
    fieldTermMeta.freq = tm->freq;
  }

  //  Term payload
  std::string payload { "(empty)" };
  if (auto const* pl = irs::get<irs::payload>(term)) {
    if (!irs::IsNull(pl->value)) {
      payload = hexBytes(pl->value);
    }
  }
  fieldTerm.payload = payload;
}

// Postings use segment-local doc ids. When `pk_by_doc` is non-null, annotate
// with @_PK (ArangoDB LocalDocumentId).
void printTermPostings(irs::field_meta const& meta,
                       irs::term_iterator const& term,
                       size_t const segment_index,
                       DocPkMap const* pk_by_doc,
                       std::vector<TermPostings>& postingsArr) {
  irs::IndexFeatures const ff = meta.index_features;

  // bool const field_has_pos =
  //     (ff & irs::IndexFeatures::POS) != irs::IndexFeatures::NONE;

  // Intersected with `ff` inside the postings reader; ask for all optional
  // layers this tool can print.
  auto posts =
      term.postings(ff & (irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
                          irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY));
  auto* freq = irs::get<irs::frequency>(*posts);
  // position::next() is non-const; irs::get returns const pointer (tests use
  // const_cast the same way).
  auto* pos = const_cast<irs::position*>(irs::get<irs::position>(*posts));

  /*
  
    terms: [
      {
        term: "Boston",
        postings: [
          {
            docId: "d1",
            positions: []
          },
          {
            docId: "d2",
            positions: []
          }
        ]
      },
  */
  //  postings (per doc: in-doc frequency, 1-based term positions in field)
  while (posts->next()) {

    postingsArr.push_back({});
    auto& postingsArrItem = *postingsArr.rbegin();

    //  document id
    postingsArrItem.docId = printSegmentLocalDocId(segment_index, posts->value(), pk_by_doc);

    //  frequency
    if (freq != nullptr) {
      postingsArrItem.inDocFreq = freq->value;
    }

    //  TODO: positions []
    #if 0
    {
      while (pos->next()) {
        VPackObjectWrapper posObj(positionsArr.get());
        posObj["position"] = VPackValue(pos->value());

        VPackObjectWrapper streamOffsetsObj(posObj.get(), "stream_offsets");
        if (auto const* off = irs::get<irs::offset>(*pos)) {
          streamOffsetsObj["start"] = VPackValue(off->start);
          streamOffsetsObj["end"] = VPackValue(off->end);
        }
      }
    }
    #endif
  }
}

void processSegmentField(irs::field_iterator::ptr& fields, size_t segmentIndex,
                         DocPkMap const* pk_by_doc,
                         IndexedField& indexedField) {

  /*
    fields: [
      {
        name: "",
        fieldHeader: {
           ..
        }
        fieldIndexFeatures: [
           ..
        ],
        terms: [{
            term: "Boston",
            postings: [
              {
                docId: "d1",
                prop1: "p1"
              },
              {
                docId: "d2",
                prop1: "p2"
              }]
          }]
        }
      ]
  */
  auto& field = fields->value();
  auto& meta = field.meta();
  std::string_view const storage_name{meta.name};
  ArangoFieldTypeInfo const detected = detectArangoFieldType(storage_name);

  indexedField.name = meta.name;
  printArangoFieldHeader(storage_name, detected, indexedField.fieldHeader);

  //  FieldIndexFeatures
  auto minTerm = irs::ViewCast<char>(field.min());
  auto maxTerm = irs::ViewCast<char>(field.max());

  auto& fieldIndexFeatures = indexedField.fieldIndexFeatures;
  fieldIndexFeatures.minTerm = minTerm;
  fieldIndexFeatures.maxTerm = maxTerm;
  fieldIndexFeatures.termsCount = field.size();
  fieldIndexFeatures.docsCount = field.docs_count();

  auto term = field.iterator(irs::SeekMode::NORMAL);
  auto& indexedTerms = indexedField.terms;

  for (; term->next();) {
    indexedTerms.push_back({});
    term->read();

    //  Term info
    printTermLine(*term, detected.kind, *indexedTerms.rbegin());
    printTermPostings(meta, *term, segmentIndex, pk_by_doc, indexedTerms.rbegin()->postings);
  }
}

void processSegment(const irs::SubReader& segment, size_t segmentIndex, SegmentInfo& segmentInfo) {

  DocPkMap const pk_by_doc = loadArangoPkByIresearchDoc(segment);

  {
    auto& meta = segment.Meta();
    segmentInfo.byteSize = meta.byte_size;
    segmentInfo.segmentId = segmentIndex;
    segmentInfo.numDocs = meta.docs_count;
    segmentInfo.numLiveDocs = meta.live_docs_count;
    segmentInfo.name = meta.name;

    //--------------------------------
    //  Fields array
    //--------------------------------
    {
      for (auto fields = segment.fields(); fields->next();) {
        segmentInfo.fields.push_back({});
        processSegmentField(fields, segmentIndex, &pk_by_doc, *segmentInfo.fields.rbegin());
      }
    }
    //--------------------------------
    
    // //--------------------------------
    // //  Column store [TODO]
    // //  Enable this code again
    // //--------------------------------
    // b.add("columns", VPackValue(VPackValueType::Array));
    // for (auto columns = segment.columns(); columns->next();) {
      
    //   b.add(VPackValue(VPackValueType::Object));
    //   auto& columnReader = columns->value();
      
    //   b.add("id", VPackValue(columnReader.id()));
    //   b.add("name", VPackValue(columnReader.name()));
      
    //   auto it = columnReader.iterator(irs::ColumnHint::kConsolidation);
    //   auto* payload = irs::get<irs::payload>(*it);
    //   auto* doc = irs::get<irs::document>(*it);
      
    //   if (!payload || !doc) {
    //     return;
    //   }
      
    //   b.add("documents", VPackValue(VPackValueType::Array));
    //   while (it->next()) {
    //     b.add(VPackValue(VPackValueType::Object));
    //     b.add("id", VPackValue(doc->value));
    //     b.add("payload", VPackValue(irs::ViewCast<char>(payload->value)));
    //     b.close();
    //   }
    //   b.close();
    //   b.close();
    // }
    // b.close(); // Close columns array
    // //--------------------------------
    
    // b.close(); // Close Segment array element
  }
}

void readIndexData(const std::filesystem::path& indexPath, IndexInfo& indexInfo) {
  irs::FSDirectory dir(indexPath);
  auto reader = irs::DirectoryReader(dir);

    indexInfo.segmentsCount = reader.size();
    indexInfo.numDocs = reader.docs_count();
    indexInfo.numLiveDocs = reader.live_docs_count();

    size_t segmentId = 0;
    indexInfo.segments.reserve(reader.size());
    for (auto& segment : reader) {
      indexInfo.segments.push_back({});
      processSegment(segment, segmentId++, *indexInfo.segments.rbegin());
    }
}

int main(int argc, char* argv[]) {

  if (argc < 2) {
    cout << "index path missing" << endl;
    return 0;
  }

  IndexInfo indexInfo;
  readIndexData(argv[1], indexInfo);

  VPackBuilder b;
  toVelocyPack(indexInfo, b);
  cout << b.toJson() << endl;
  return 0;
}
