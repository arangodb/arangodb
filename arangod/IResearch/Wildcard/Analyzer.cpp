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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#include "Assertions/Assert.h"
#include "Basics/DownCast.h"
#include "IResearch/Wildcard/Analyzer.h"

#include "analysis/token_streams.hpp"
#include "utils/bytes_utils.hpp"
#include "utils/vpack_utils.hpp"

#include <velocypack/Builder.h>

namespace arangodb::iresearch::wildcard {
namespace {

constexpr std::string_view kNgramSize = "ngramSize";
constexpr std::string_view kParseError =
    ", failed to parse options for wildcard analyzer";
constexpr size_t kMinNgram = 2;

bool ParseNgramSize(velocypack::Slice input, size_t& ngramSize) {
  IRS_ASSERT(input.isObject());
  input = input.get(kNgramSize);
  if (!input.isNumber<size_t>()) {
    IRS_LOG_ERROR(
        absl::StrCat(kNgramSize, " attribute must be size_t", kParseError));
    return false;
  }
  ngramSize = input.getNumber<size_t>();
  if (ngramSize < kMinNgram) {
    IRS_LOG_ERROR(absl::StrCat(kNgramSize, " attribute must be at least ",
                               kMinNgram, kParseError));
    return false;
  }
  return true;
}

bool ParseOptions(velocypack::Slice slice, Analyzer::Options& options) {
  if (!slice.isObject()) {
    return false;
  }
  if (!ParseNgramSize(slice, options.ngramSize)) {
    return false;
  }
  if (!irs::analysis::analyzers::MakeAnalyzer(slice, options.analyzer)) {
    IRS_LOG_ERROR(absl::StrCat("Invalid analyzer definition in ",
                               irs::slice_to_string(slice), kParseError));
    return false;
  }
  return true;
}

irs::analysis::analyzer::ptr MakeImpl(velocypack::Slice slice) {
  if (Analyzer::Options opts; ParseOptions(slice, opts)) {
    return std::make_unique<Analyzer>(std::move(opts));
  }
  return {};
}

bool NormalizeImpl(velocypack::Slice input, velocypack::Builder& output) {
  if (!input.isObject()) {
    return false;
  }
  size_t ngram_size = 0;
  if (!ParseNgramSize(input, ngram_size)) {
    return false;
  }
  velocypack::ObjectBuilder scope{&output};
  output.add(kNgramSize, velocypack::Value{ngram_size});
  if (!irs::analysis::analyzers::NormalizeAnalyzer(input, output)) {
    IRS_LOG_ERROR(absl::StrCat("Invalid analyzer definition in ",
                               irs::slice_to_string(input), kParseError));
    return false;
  }
  return true;
}

static constexpr std::string_view kFill = "00000\xFF";

constexpr std::string_view fill(size_t len) noexcept {
  return {kFill.data() + kFill.size() - len, len};
}

irs::byte_type const* nextUTF8(irs::byte_type const* it,
                               irs::byte_type const* end) noexcept {
  const uint32_t cp_start = *it++;
  if (IRS_UNLIKELY(cp_start >= 0b1000'0000)) {
    if (cp_start < 0b1110'0000) {
      ++it;
    } else if (cp_start < 0b1111'0000) {
      it += 2;
    } else if (cp_start < 0b1111'1000) {
      it += 3;
    }
    if (it > end) {
      it = end;
    }
  }
  return it;
}

}  // namespace

bool Analyzer::normalize(std::string_view args, std::string& definition) {
  if (args.empty()) {
    IRS_LOG_ERROR(absl::StrCat("Empty arguments", kParseError));
    return false;
  }
  velocypack::Slice input{reinterpret_cast<const uint8_t*>(args.data())};
  velocypack::Builder output;
  if (!NormalizeImpl(input, output)) {
    return false;
  }
  definition.assign(output.slice().startAs<char>(), output.slice().byteSize());
  return true;
}

irs::analysis::analyzer::ptr Analyzer::make(std::string_view args) {
  if (args.empty()) {
    IRS_LOG_ERROR(absl::StrCat("Empty arguments", kParseError));
    return {};
  }
  velocypack::Slice slice{reinterpret_cast<const uint8_t*>(args.data())};
  return MakeImpl(slice);
}

irs::attribute* Analyzer::get_mutable(irs::type_info::type_id type) {
  if (type == irs::type<irs::offset>::id()) {
    return nullptr;
  }
  return _ngram->get_mutable(type);
}

bool Analyzer::reset(std::string_view data) {
  if (!_analyzer->reset(data)) {
    return false;
  }
  _terms.clear();
  while (_analyzer->next()) {
    auto const size = _term->value.size();
    if (size > std::numeric_limits<int32_t>::max()) {
      // TODO(MBkkt) icu doesn't support more
      IRS_LOG_WARN(
          absl::StrCat("too long input for wildcard analyzer: ", size));
      continue;
    }
    // TODO(MBkkt) We can add offsets here
    auto const idx = _terms.size();
    absl::StrAppend(
        &_terms,
        fill(irs::bytes_io<uint32_t>::vsize(static_cast<uint32_t>(size)) + 1),
        irs::ViewCast<char>(_term->value), fill(1));
    auto* data = begin() + idx;
    irs::vwrite<uint32_t>(data, static_cast<uint32_t>(size));
  }
  _termsBegin = begin();
  _termsEnd = _termsBegin + _terms.size();
  return _termsBegin != _termsEnd;
}

irs::bytes_view Analyzer::store(irs::token_stream* ctx,
                                velocypack::Slice slice) {
  auto& impl = basics::downCast<Analyzer>(*ctx);
  return irs::ViewCast<irs::byte_type>(std::string_view{impl._terms});
}

bool Analyzer::next() {
  if (IRS_LIKELY(_ngram->next())) {
    return true;
  }
  if (auto size = _ngramTerm->value.size(); size > 1) {
    auto* begin = _ngramTerm->value.data();
    auto* end = begin + _ngramTerm->value.size();
    begin = nextUTF8(begin, end);
    _ngramTerm->value = {begin, end};
    if (_ngramTerm->value.size() > 1) {
      return true;
    }
  }
  if (_termsBegin == _termsEnd) {
    return false;
  }
  auto const size = irs::vread<uint32_t>(_termsBegin) + 2U;
  irs::bytes_view term{_termsBegin, size};
  _ngram->reset(irs::ViewCast<char>(term));
  _termsBegin += size;
  if (!_ngram->next()) {
    _ngramTerm->value = term;
  }
  return true;
}

Analyzer::Analyzer(Options&& options) noexcept
    : _analyzer{std::move(options.analyzer)} {
  if (!_analyzer) {
    // Fallback to default implementation
    _analyzer = std::make_unique<irs::string_token_stream>();
  }
  auto ptr = Ngram::make({
      options.ngramSize,
      options.ngramSize,
      false,
      Ngram::InputType::UTF8,
      {},
      {},
  });
  _ngram = decltype(_ngram){basics::downCast<Ngram>(ptr.release())};
  TRI_ASSERT(_ngram);
  _term = irs::get<irs::term_attribute>(*_analyzer);
  TRI_ASSERT(_term);
  _ngramTerm = irs::get_mutable<irs::term_attribute>(_ngram.get());
  TRI_ASSERT(_ngramTerm);
}

}  // namespace arangodb::iresearch::wildcard
