////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Ewout Prangsma
////////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef ARANGO_CXX_DRIVER_VPACK_INIT_H
#define ARANGO_CXX_DRIVER_VPACK_INIT_H

#include <memory>

#include <fuerte/types.h>
#include <velocypack/AttributeTranslator.h>
#include <velocypack/Options.h>

namespace arangodb { namespace fuerte { inline namespace v1 { namespace helper {

// initializes an ArangoDB conformant attribute translator with the
// default velocypack options. Only use once
class VpackInit {
  std::unique_ptr<arangodb::velocypack::AttributeTranslator> _translator;

 public:
  VpackInit() : _translator(new arangodb::velocypack::AttributeTranslator) {
    _translator->add("_key", uint8_t(1));
    _translator->add("_rev", uint8_t(2));
    _translator->add("_id", uint8_t(3));
    _translator->add("_from", uint8_t(4));
    _translator->add("_to", uint8_t(5));
    _translator->seal();
    arangodb::velocypack::Options::Defaults.attributeTranslator =
        _translator.get();
  }
};

}}}}  // namespace arangodb::fuerte::v1::helper

#endif
