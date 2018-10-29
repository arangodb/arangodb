////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_ATTRIBUTES_H
#define ARANGOD_IRESEARCH__IRESEARCH_ATTRIBUTES_H 1

#include "utils/attributes.hpp"
#include "velocypack/Builder.h"

NS_BEGIN(arangodb)
NS_BEGIN(transaction)

class Methods; // forward declaration

NS_END // transaction
NS_END // arangodb

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)
NS_BEGIN(attribute)

//////////////////////////////////////////////////////////////////////////////
/// @brief contains the path to the attribute encoded as a jSON array
//////////////////////////////////////////////////////////////////////////////
struct AttributePath: irs::basic_stored_attribute<arangodb::velocypack::Builder> {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();
};

//////////////////////////////////////////////////////////////////////////////
/// @brief contains a pointer to the current transaction
//////////////////////////////////////////////////////////////////////////////
struct Transaction: irs::basic_attribute<arangodb::transaction::Methods&> {
  DECLARE_ATTRIBUTE_TYPE();

  explicit Transaction(arangodb::transaction::Methods& trx);
};

NS_END // attribute
NS_END // iresearch
NS_END // arangodb

#endif