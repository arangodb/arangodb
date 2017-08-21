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

#include "Transaction/Methods.h"

#include "IResearchAttributes.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

NS_BEGIN(attribute)

// -----------------------------------------------------------------------------
// --SECTION--                                                     AttributePath
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(AttributePath);
DEFINE_ATTRIBUTE_TYPE(AttributePath);
DEFINE_FACTORY_DEFAULT(AttributePath);

// -----------------------------------------------------------------------------
// --SECTION--                                                       Transaction
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(Transaction);
DEFINE_ATTRIBUTE_TYPE(Transaction);
DEFINE_FACTORY_DEFAULT(Transaction);

NS_END // attribute

NS_BEGIN(stored_attribute)

// -----------------------------------------------------------------------------
// --SECTION--                                                     AttributePath
// -----------------------------------------------------------------------------

REGISTER_ATTRIBUTE(AttributePath);
DEFINE_ATTRIBUTE_TYPE(AttributePath);
DEFINE_FACTORY_DEFAULT(AttributePath);

NS_END // stored_attribute

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------