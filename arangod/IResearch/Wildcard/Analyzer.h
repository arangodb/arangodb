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

#pragma once

#include "Assertions/Assert.h"
#include "analysis/analyzer.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/ngram_token_stream.hpp"

#include <velocypack/Slice.h>

namespace arangodb::iresearch::wildcard {

class Analyzer final : public irs::analysis::TypedAnalyzer<Analyzer>,
                       private irs::util::noncopyable {
  using Ngram = irs::analysis::ngram_token_stream<
      irs::analysis::ngram_token_stream_base::InputType::UTF8>;

 public:
  struct Options {
    analyzer::ptr baseAnalyzer;
    size_t ngramSize = 3;
  };

  static constexpr std::string_view type_name() noexcept { return "wildcard"; }
  static bool normalize(std::string_view args, std::string& definition);
  static analyzer::ptr make(std::string_view args);

  explicit Analyzer(Options&& options) noexcept;

  irs::attribute* get_mutable(irs::type_info::type_id type) final;

  bool reset(std::string_view data) final;

  static irs::bytes_view store(irs::token_stream* ctx, velocypack::Slice slice);

  bool next() final;

  auto& ngram() const noexcept {
    TRI_ASSERT(_ngram);
    return *_ngram;
  }

 private:
  irs::byte_type* begin() noexcept {
    return reinterpret_cast<irs::byte_type*>(_terms.data());
  }

  analyzer::ptr _analyzer;
  std::unique_ptr<Ngram> _ngram;
  irs::term_attribute const* _term{};
  irs::term_attribute* _ngramTerm{};
  std::string _terms;
  irs::byte_type const* _termsBegin{};
  irs::byte_type const* _termsEnd{};
};

}  // namespace arangodb::iresearch::wildcard
