////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_IDENTITY_ANALYZER
#define ARANGODB_IRESEARCH__IRESEARCH_IDENTITY_ANALYZER 1

#include "analysis/analyzer.hpp"
#include "analysis/token_attributes.hpp"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

namespace arangodb {
namespace iresearch {

class IdentityAnalyzer final : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "identity";
  }

  static bool normalize(const irs::string_ref& /*args*/, std::string& out);

  static ptr make(irs::string_ref const& /*args*/);

  IdentityAnalyzer() noexcept;

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override;

  virtual bool next() noexcept override {
    auto const empty = _empty;

    _empty = true;

    return !empty;
  }

  virtual bool reset(irs::string_ref const& data) noexcept override {
    _empty = false;
    _term.value = irs::ref_cast<irs::byte_type>(data);

    return true;
  }

 private:
  irs::term_attribute _term;
  irs::increment _inc;
  bool _empty;
}; // IdentityAnalyzer

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_IDENTITY_ANALYZER

