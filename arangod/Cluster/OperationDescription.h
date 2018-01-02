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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include <string>

namespace arangodb {
namespace maintenance {

enum Type { CREATE_DATASE = 2, DROP_DATABASE,
            CREATE_COLLECTION, DROP_COLLECTION,
            CREATE_INDEX, DROP_INDEX };

/// @brief Maintenance operation description card
struct OperationDescription {

  OperationDescription(Type const&, std::string const&);
  virtual ~OperationDescription();

  Type _type;
  std::string _name;
};

}}

namespace std {
template<> struct hash<arangodb::maintenance::OperationDescription> {
  typedef arangodb::maintenance::OperationDescription argument_t;
  typedef std::size_t result_t;
  result_t operator()(argument_t const& a) const noexcept;
};
}

