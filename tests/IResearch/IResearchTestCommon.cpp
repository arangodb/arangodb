////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "IResearchTestCommon.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <analysis/analyzers.hpp>

#include "IResearch/VelocyPackHelper.h"

TestAnalyzer::TestAnalyzer()
    : irs::analysis::analyzer(irs::type<TestAnalyzer>::get()) {}

bool TestAnalyzer::reset(const iresearch::string_ref& data) {
  _data = irs::ref_cast<irs::byte_type>(data);
  return true;
}

bool TestAnalyzer::next() {
  if (_data.empty()) return false;

  _term.value = irs::bytes_ref(_data.c_str(), 1);
  _data = irs::bytes_ref(_data.c_str() + 1, _data.size() - 1);
  return true;
}

bool TestAnalyzer::normalize(const iresearch::string_ref& args,
                             std::string& definition) {
  // same validation as for make,
  // as normalize usually called to sanitize data before make
  auto slice = arangodb::iresearch::slice(args);
  if (slice.isNull()) throw std::exception();
  if (slice.isNone()) return false;

  arangodb::velocypack::Builder builder;
  if (slice.isString()) {
    VPackObjectBuilder scope(&builder);
    arangodb::iresearch::addStringRef(builder, "args",
                                      arangodb::iresearch::getStringRef(slice));
  } else if (slice.isObject() && slice.hasKey("args") &&
             slice.get("args").isString()) {
    VPackObjectBuilder scope(&builder);
    arangodb::iresearch::addStringRef(
        builder, "args", arangodb::iresearch::getStringRef(slice.get("args")));
  } else {
    return false;
  }

  definition = builder.buffer()->toString();

  return true;
}

auto TestAnalyzer::make(const iresearch::string_ref& args) -> ptr {
  auto slice = arangodb::iresearch::slice(args);
  if (slice.isNull()) throw std::exception();
  if (slice.isNone()) return nullptr;
  PTR_NAMED(TestAnalyzer, ptr);
  return ptr;
}

irs::attribute* TestAnalyzer::get_mutable(
    irs::type_info::type_id type) noexcept {
  if (type == irs::type<TestAttribute>::id()) {
    return &_attr;
  }
  if (type == irs::type<irs::increment>::id()) {
    return &_increment;
  }
  if (type == irs::type<irs::term_attribute>::id()) {
    return &_term;
  }
  return nullptr;
}

REGISTER_ATTRIBUTE(
    TestAttribute);  // required to open reader on segments with analyzed fields

REGISTER_ANALYZER_VPACK(TestAnalyzer, TestAnalyzer::make,
                        TestAnalyzer::normalize);
