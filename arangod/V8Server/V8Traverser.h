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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_V8_SERVER_V8_TRAVERSER_H
#define ARANGOD_V8_SERVER_V8_TRAVERSER_H 1

#include "Basics/VelocyPackHelper.h"
#include "VocBase/Traverser.h"

namespace arangodb {

namespace velocypack {
class Slice;
}
}

namespace arangodb {
namespace traverser {

// A collection of shared options used in several functions.
// Should not be used directly, use specialization instead.
struct BasicOptions {

  arangodb::Transaction* _trx;

 protected:

  explicit BasicOptions(arangodb::Transaction* trx)
      : _trx(trx) {}

  virtual ~BasicOptions() {
  }

 public:
  std::string start;


 public:

  arangodb::Transaction* trx() { return _trx; }

};

struct ShortestPathOptions : BasicOptions {
 public:
  std::string direction;
  bool useWeight;
  std::string weightAttribute;
  double defaultWeight;
  bool bidirectional;
  bool multiThreaded;
  std::string end;
  arangodb::velocypack::Builder startBuilder;
  arangodb::velocypack::Builder endBuilder;

  explicit ShortestPathOptions(arangodb::Transaction* trx);

  void setStart(std::string const&);
  void setEnd(std::string const&);

  arangodb::velocypack::Slice getStart() const;
  arangodb::velocypack::Slice getEnd() const;

};

}
}

#endif
