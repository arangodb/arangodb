////////////////////////////////////////////////////////////////////////////////
/// @brief Avocado Query Language
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      FLUENT QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AQL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fluent query constructor
////////////////////////////////////////////////////////////////////////////////

function AvocadoFluentQuery2 (collection) {
  this._query = null;
  this._execution = null;

  this._primary = collection;
  this._select = null;
  this._joins = null;
  this._where = null;
  this._skip = null;
  this._limit = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AQL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a fluent query
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.copyQuery = function() {
  var copy;

  copy = new AvocadoFluentQuery2(this._primary);

  copy._select = this._select;
  copy._joins = this._joins;
  copy._where = this._joins;
  copy._skip = this._skip;
  copy._limit = this._limit;

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a fluent query
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.buildQuery = function() {
  this._query = AQL_SELECT(this._select,
                           "$",
                           this._primary,
                           this._joins,
                           this._where,
                           this._skip,
                           this._limit);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a fluent query
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.execute = function () {
  if (this._query == null) {
    this.buildQuery();
    this._execution = null;
  }

  if (this._execution == null) {
    this._execution = this._query.execute();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AQL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
///
/// @FUN{hasNext()}
///
/// The @FN{hasNext} operator returns @LIT{true}, if the cursor still has
/// documents.  In this case the next document can be accessed using the
/// @FN{next} operator, which will advance the cursor.
///
/// @verbinclude fluent3
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.hasNext = function () {
  this.execute();

  return this._execution.hasNext();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
///
/// @FUN{next()}
///
/// If the @FN{hasNext} operator returns @LIT{true}, if the cursor still has
/// documents.  In this case the next document can be accessed using the @FN{next}
/// operator, which will advance the cursor.
///
/// @verbinclude fluent28
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.next = function () {
  this.execute();

  return this._execution.next();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
///
/// @FUN{nextRef()}
///
/// If the @FN{hasNext} operator returns @LIT{true}, if the cursor still has
/// documents.  In this case the next document reference can be
/// accessed using the @FN{nextRef} operator, which will advance the
/// cursor.
///
/// @verbinclude fluent51
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.nextRef = function () {
  this.execute();

  return this._execution.nextRef();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief uses the next document
///
/// @FUN{useNext()}
///
/// If the @FN{hasNext} operator returns @LIT{true}, then the cursor still has
/// documents.  In this case the next document can be skipped using the
/// @FN{useNext} operator, which will advance the cursor and return @LIT{true}.
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.useNext = function () {
  this.execute();

  return this._execution.useNext();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skips an existing query
///
/// @FUN{skip(@FA{number})}
///
/// Skips the first @FA{number} documents.
///
/// @verbinclude fluent31
///
/// The corresponding AQL queries would be:
///
/// @verbinclude fluent31-aql
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.T_skip = function (skip) {
  var copy;

  copy = this.copyQuery();

  // limit already set
  if (copy._limit != null) {
    for (var i = 0;  i < skip && copy.hasNext();  ++i) {
      copy.useNext();
    }
  }

  // no limit, no skip, use new skip
  else if (copy._skip == null) {
    copy._skip = skip;
  }

  // no limit, old skip, use both skips
  else {
    copy._skip = copy._skip + skip;
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief limits an existing query
///
/// @FUN{limit(@FA{number})}
///
/// Limits a result to the first @FA{number} documents. Specifying a limit of
/// @CODE{0} returns no documents at all. If you do not need a limit, just do
/// not add the limit operator. If you specifiy a negtive limit of @CODE{-n},
/// this will return the last @CODE{n} documents instead.
///
/// @verbinclude fluent30
///
/// The corresponding AQL queries would be:
///
/// @verbinclude fluent30-aql
////////////////////////////////////////////////////////////////////////////////

AvocadoFluentQuery2.prototype.T_limit = function (limit) {
  var copy;

  copy = this.copyQuery();

  // no old limit or new limit is 0, add limit
  if (copy._limit == null || limit == 0) {
    copy._limit = limit;
  }

  // old limit is 0, do not change
  else if (copy._limit == 0) {
  }

  // no change
  else if (copy._limit == limit) {
    copy._limit = limit;
  }

  // new limit is positive
  else if (0 < limit) {

    // new limit is positive and smaller
    if (limit < copy._limit) {
      copy._limit = limit;
    }

    // new limit is positive and old limit is neagtive
    else if (copy._limit < 0) {
      argh();
    }
  }

  // new limit is negative
  else {

    // new limit is negative and larger
    if (copy._limit <= limit) {
      copy._limit = limit;
    }

    // new limit is negative and old limit is positive
    else if (0 < copy._limit) {
      argh();
    }
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup AQL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all elements
///
/// @FUN{all()}
///
/// Selects all documents of a collection.
///
/// @verbinclude fluent23
///
/// The corresponding AQL query would be:
///
/// @verbinclude fluent23-aql
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.T_all = function () {
  return new AvocadoFluentQuery2(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
///
/// @FUN{document(@FA{document-identifier})}
///
/// The @FN{document} operator finds a document given it's identifier.  It returns
/// the empty result set or a result set containing the document with document
/// identifier @FA{document-identifier}.
///
/// @verbinclude fluent54
///
/// The corresponding AQL query would be:
///
/// @verbinclude fluent54-aql
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.T_limit = function (id) {
  var query;

  query = new AvocadoFluentQuery2(this);
  query._where = AQL_WHERE_PRIMARY_INDEX(query._primary, id);

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief limits an existing query
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.T_limit = function (limit) {
  var query;

  query = new AvocadoFluentQuery2(this);
  query._limit = limit;

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skips an existing query
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.T_skip = function (skip) {
  var query;

  query = new AvocadoFluentQuery2(this);
  query._skip = skip;

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skips an existing query
////////////////////////////////////////////////////////////////////////////////

AvocadoCollection.prototype.T_toArray = function () {
  var cursor;
  var result;

  cursor = this.T_all();
  result = [];

  while (cursor.hasNext()) {
    result.push(cursor.next());
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
