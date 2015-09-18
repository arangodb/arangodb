////////////////////////////////////////////////////////////////////////////////
/// @brief index-operator (operators used for skiplists and other indexes)
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_INDEX_OPERATORS_INDEX__OPERATOR_H
#define ARANGODB_INDEX_OPERATORS_INDEX__OPERATOR_H 1

#include "Basics/Common.h"
#include "Basics/json.h"
#include "VocBase/shaped-json.h"
#include "VocBase/vocbase.h"

class VocShaper;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// Explanation of what an index operator is capable of and how one has to 
/// use it:
///
/// An index operator is a binary tree of AND, OR or NOT nodes (with 2, 2
/// and 1 child respectively, of type TRI_logical_index_operator_t) in 
/// which the leaves are relational nodes (type TRI_relation_index_operator_t).
/// These leaf nodes have a list of parameters. The length of this
/// parameter list indicates how many of the attributes of the index
/// are used, starting from the first and not skipping any. Depending
/// on the type of index, only certain comparisons are allowed.
///
/// For example for a hash index, only the EQ relation is supported and 
/// there must be exactly as many parameters as attributes used in the
/// hash index. This is clear from how a hash index works.
///
/// For a skiplist index, an EQ node can have a parameter list of length
/// k which must be at least 1 and at most the number of attributes
/// given in the index. NE nodes are not (yet) supported. The other four
/// relational nodes need a parameter list as above and the meaning is
/// that the values 1..k-1 mean equality and k mean the relation given.
/// This is again clear by how a skiplist index works, since for one
/// relational condition it just wants to do a single lookup to find
/// the boundary.
///
/// Note that OR and NOT nodes are not (yet) supported by the skiplist index.
/// 
/// Finally, note that the boundaries of LE, GE, LT and GT nodes can not
/// be arrays or lists at this stage since these would potentially give
/// rise to new shapes in the shaper which we must not allow. For EQ nodes,
/// arbitrary JSON values are allowed as parameters.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief select clause type
////////////////////////////////////////////////////////////////////////////////

typedef enum {

  TRI_EQ_INDEX_OPERATOR,
  TRI_NE_INDEX_OPERATOR,
  TRI_LE_INDEX_OPERATOR,
  TRI_LT_INDEX_OPERATOR,
  TRI_GE_INDEX_OPERATOR,
  TRI_GT_INDEX_OPERATOR,

  TRI_AND_INDEX_OPERATOR
}
TRI_index_operator_type_e;



////////////////////////////////////////////////////////////////////////////////
/// @brief operands and operators
////////////////////////////////////////////////////////////////////////////////

class TRI_index_operator_t {
  public: 
    TRI_index_operator_type_e const _type;
    VocShaper const* _shaper;

    TRI_index_operator_t (
      TRI_index_operator_type_e const type,
      VocShaper const* shaper
    ) : _type(type),
        _shaper(shaper) {
    }

    virtual ~TRI_index_operator_t () {
    } 
};


//................................................................................
// Storage for NOT / AND / OR logical operators used in skip list indexes and others
// It is a binary or unary operator
//................................................................................

class TRI_logical_index_operator_t : public TRI_index_operator_t {
  public:
    TRI_index_operator_t* _left;  // could be a relation or another logical operator
    TRI_index_operator_t* _right; // could be a relation or another logical operator or null

    TRI_logical_index_operator_t (
      TRI_index_operator_type_e type,
      VocShaper* shaper,
      TRI_index_operator_t* left,
      TRI_index_operator_t* right
    ) : TRI_index_operator_t(type, shaper),
        _left(left),
        _right(right)
    {};

    ~TRI_logical_index_operator_t () {
      delete _left;
      delete _right;
    };
};


//................................................................................
// Storage for relation operator, e.g. <, <=, >, >=, ==, in
//................................................................................

class TRI_relation_index_operator_t : public TRI_index_operator_t {
  public:
    TRI_json_t* _parameters;    // parameters with which this relation was called with
    TRI_shaped_json_t* _fields; // actual data from the parameters converted from
                                      // a json array to a shaped json array
    size_t _numFields;          // number of fields in the array above

    TRI_relation_index_operator_t (
      TRI_index_operator_type_e const type,
      VocShaper const* shaper,
      TRI_json_t* parameters,
      TRI_shaped_json_t* fields,
      size_t numFields
    ) : TRI_index_operator_t(type, shaper),
        _parameters(parameters),
        _fields(fields),
        _numFields(numFields) {
    }

    ~TRI_relation_index_operator_t ();

};


////////////////////////////////////////////////////////////////////////////////
/// @brief create a new index operator of the specified type
///
/// note that the index which uses these operators  will take ownership of the
/// json parameters passed to it
////////////////////////////////////////////////////////////////////////////////

TRI_index_operator_t* TRI_CreateIndexOperator (TRI_index_operator_type_e,
                                               TRI_index_operator_t*,
                                               TRI_index_operator_t*,
                                               TRI_json_t*,
                                               VocShaper*,
                                               size_t);
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
