////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VIEWS_VIEW_ITERATOR_H
#define ARANGOD_VIEWS_VIEW_ITERATOR_H 1

#include "Basics/Common.h"
#include "StorageEngine/DocumentIdentifierToken.h"
#include "VocBase/ManagedDocumentResult.h"

namespace arangodb {
class ViewImplementation;

namespace transaction {
class Methods;
}
namespace velocypack {
class Slice;
}

class ViewIterator {
 public:
  typedef std::function<void(DocumentIdentifierToken const& token)> TokenCallback;

  typedef std::function<void(DocumentIdentifierToken const& token, arangodb::velocypack::Slice extra)> ExtraCallback;

  ViewIterator() = delete;
  ViewIterator(ViewIterator const&) = delete;
  ViewIterator& operator=(ViewIterator const&) = delete;

  ViewIterator(ViewImplementation* view, transaction::Methods* trx)
      : _view(view), _trx(trx) {}
  virtual ~ViewIterator() {}

  virtual char const* typeName() const = 0;

  transaction::Methods* transaction() const { return _trx; }
  ViewImplementation* view() const { return _view; }

  // fetches the next limit results (at most). the iterator is free to
  // produce less results than requested.
  // the iterator must call the ResultCallback function for each result.
  // this function will usually add the iterator's result slice to a query
  // result. it will copy the slice so the iterator must only ensure the
  // slice stays valid while the callback is executed.
  // the method must return `true` if there may be more results available.
  // if no more results are available, the method must return `false`.
  virtual bool next(TokenCallback const& callback, size_t limit) = 0;

  // reset the iterator to its beginning
  virtual void reset();

  virtual bool nextExtra(ExtraCallback const& callback, size_t limit);
  virtual bool hasExtra() const;

  virtual void skip(uint64_t count, uint64_t& skipped);  // same as IndexIterator API

  bool readDocument(arangodb::DocumentIdentifierToken const& token,
                    arangodb::ManagedDocumentResult& result) const;

 protected:
  ViewImplementation* _view;
  transaction::Methods* _trx;
};

}  // namespace arangodb

#endif
