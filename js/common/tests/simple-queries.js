////////////////////////////////////////////////////////////////////////////////
/// @brief tests for "aql.js"
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlTestSuite () {
  var collection = null;
  var documents = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  function setUp () {
    this.collection = internal.db.UnitTestsCollection;

    if (this.collection.count() == 0) {
      this.collection.save({ name: "one", age: 1 });
      this.collection.save({ name: "two", age: 2 });
      this.collection.save({ name: "three", age: 3 });
      this.collection.save({ name: "four", age: 4 });
      this.collection.save({ name: "five", age: 5 });
    }

    this.documents = this.collection.T_toArray();
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  function tearDown () {
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks that collection contains expected number of elements
////////////////////////////////////////////////////////////////////////////////

  function testSizeOfTestCollection () {
    assertEqual(5, this.collection.T_toArray().length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <collection>.toArray()
////////////////////////////////////////////////////////////////////////////////

  function testCollectionToArray () {
    var query;

    assertEqual(5, this.collection.T_toArray().length);

    try {
      query = this.collection.T_all();
      query.hasNext();
      query.toArray();

      fail("trying to execute an executing query");
    }
    catch (err) {
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <collection>.skip(<n>)
////////////////////////////////////////////////////////////////////////////////

  function testCollectionSkip () {
    assertEqual(5, this.collection.T_skip(0).toArray().length);
    assertEqual(4, this.collection.T_skip(1).toArray().length);
    assertEqual(3, this.collection.T_skip(2).toArray().length);
    assertEqual(5, this.collection.T_skip(null).toArray().length);

    try {
      this.collection.T_skip(-1);
      fail("expected exception for skip(-1)");
    }
    catch (err) {
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <collection>.limit(<n>)
////////////////////////////////////////////////////////////////////////////////

  function testCollectionLimit () {
    assertEqual(1, this.collection.T_limit(1).toArray().length);
    assertEqual(1, this.collection.T_limit(-1).toArray().length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <collection>.document(<id>)
////////////////////////////////////////////////////////////////////////////////

  function testCollectionDocument () {
    var id = this.documents[0]._id;

    assertEqual(id, this.collection.T_document(id).next()._id);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <internal-query>.nextRef()
////////////////////////////////////////////////////////////////////////////////

  function testInternalQueryNextRef () {
    var id = this.documents[0]._id;

    assertEqual(id, this.collection.T_document(id).nextRef());
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <internal-query>.useRef()
////////////////////////////////////////////////////////////////////////////////

  function testInternalQueryUseRef () {
    var count = 0;
    var query = this.collection.T_all();

    for (var i = 0;  i < 5 && query.hasNext();  ++i, ++count) {
      query.useNext();
    }

    assertEqual(5, count);
    assertFalse(query.hasNext());
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <query>.skip(<n>)
////////////////////////////////////////////////////////////////////////////////

  function testQuerySkip () {
    var query;

    assertEqual(5, this.collection.T_all().skip(0).toArray().length);
    assertEqual(4, this.collection.T_all().skip(1).toArray().length);
    assertEqual(3, this.collection.T_all().skip(2).toArray().length);
    assertEqual(5, this.collection.T_all().skip(null).toArray().length);

    try {
      this.collection.T_all().skip(-1);
      fail("expected exception for skip(-1)");
    }
    catch (err) {
    }

    try {
      query = this.collection.T_all();
      query.hasNext();
      query.skip(1);

      fail("trying to execute an executing query");
    }
    catch (err) {
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <query>.limit()
////////////////////////////////////////////////////////////////////////////////

  function testQueryLimit () {
    var query;

    assertEqual(3, this.collection.T_all().skip(1).limit(4).skip(1).toArray().length);
    assertEqual(2, this.collection.T_all().skip(1).limit(4).limit(-2).toArray().length);
    assertEqual(2, this.collection.T_all().skip(1).limit(-4).limit(2).toArray().length);
    assertEqual(2, this.collection.T_all().skip(1).limit(3).limit(2).toArray().length);
    assertEqual(2, this.collection.T_all().skip(1).limit(-3).limit(-2).toArray().length);
    assertEqual(0, this.collection.T_all().skip(1).limit(0).limit(null).toArray().length);

    try {
      query = this.collection.T_all().skip(1).limit(0);
      query.hasNext();
      query.limit(null);

      fail("trying to execute an executing query");
    }
    catch (err) {
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief <query>.document(<id>)
////////////////////////////////////////////////////////////////////////////////

  function testQueryDocument () {
    var query;
    var id = this.documents[1]._id;

    assertEqual(id, this.collection.T_all().skip(1).limit(-4).limit(2).document(id).next()._id);

    try {
      query = this.collection.T_all().skip(1).limit(-4).limit(2);
      query.hasNext();
      query.document(id);

      fail("trying to execute an executing query");
    }
    catch (err) {
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(aqlTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
