////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_AGENCY_MOCK_H
#define ARANGODB_IRESEARCH__IRESEARCH_AGENCY_MOCK_H 1

#include "AgencyCommManagerMock.h"

namespace arangodb {
namespace consensus {

class Store;

} // consensus
} // arangod

////////////////////////////////////////////////////////////////////////////////
/// @brief specialization of GeneralClientConnectionMock returning results from
///        underlying agency store
////////////////////////////////////////////////////////////////////////////////
class GeneralClientConnectionAgencyMock: public GeneralClientConnectionMock {
 public:
  explicit GeneralClientConnectionAgencyMock(
      arangodb::consensus::Store& store,
      bool trace = false
  ) noexcept: _store(&store), _trace(trace) {
  }

 protected:
  virtual void response(arangodb::basics::StringBuffer& buffer) override;
  virtual void request(char const* data, size_t length) override;

 private:
  std::string const& action() {
    TRI_ASSERT(_path.size() == 4);
    return _path[3];
  }

  void handleRead(arangodb::basics::StringBuffer& buffer);
  void handleWrite(arangodb::basics::StringBuffer& buffer);

  arangodb::consensus::Store* _store;
  std::vector<std::string> _path;
  std::string _url;
  std::string _body;
  bool _trace;
}; // GeneralClientConnectionAgencyMock

#endif