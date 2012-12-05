/*jslint indent: 2,
         nomen: true,
         maxlen: 80 */
/*global require,
    db,
    assertEqual, assertTrue,
    ArangoCollection */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for bitarray indexes
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
/// @author Dr. O
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var console = require("console");

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function BitarrayIndexSuite() {
  var ERRORS = internal.errors;
  var cn = "UnitTestsCollectionBitarray";
  var collection = null;
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  // ...........................................................................
  // jsunity automatically calls this property before each test for you
  // ...........................................................................
  
  setUp : function () {
    internal.db._drop(cn);
    collection = internal.db._create(cn, { waitForSync : false });
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  tearDown : function () {
    collection.unload();
    collection.drop();
    internal.wait(0.0);
    collection = null;
  },
  
////////////////////////////////////////////////////////////////////////////////
/// @brief test: bitarray index creation type 1
////////////////////////////////////////////////////////////////////////////////
  
  testCreationBitarrayIndex_1a : function () {
    var idx;

    try {
      idx = collection.ensureBitarray("a");
    }
    catch(err) {
      return;
    }  
    
    throw("Bitarray index can not be constructed in this fashion");
  },
  

  
  testCreationBitarrayIndex_1b : function () {
    var idx;

    try {
      idx = collection.ensureBitarray("a",[0,1,2,3],"b");
    }
    catch(err) {
      return;
    }  
    
    throw("Bitarray index can not be constructed in this fashion");
  },

  
  testCreationBitarrayIndex_1c : function () {
    var idx;

    try {
      idx = collection.ensureBitarray("a",[0,1,2,3],"a",[4,5,6,7]);
    }
    catch(err) {
      return;
    }  
    
    throw("Bitarray index can not be constructed in this fashion");
  },

  testCreationBitarrayIndex_1d : function () {
    var idx;

    try {
      idx = collection.ensureBitarray("a",[0,1,2,3],"b",[4,5,6,7,4]);
    }
    catch(err) {
      return;
    }  
    
    throw("Bitarray index can not be constructed in this fashion");
  },


////////////////////////////////////////////////////////////////////////////////
/// @brief test: bitarray index creation type 2
////////////////////////////////////////////////////////////////////////////////
  
  testCreationBitarrayIndex_2a : function () {
    var idx;
   
    
    // .........................................................................
    // Create a bit array index with one attribute
    // .........................................................................
    
    idx = collection.ensureBitarray("a",[0,1,2,3,4,5,6,7,8,9,[]]);
    
    
    // .........................................................................
    // Got through the returned json object and check whether things are as 
    // they should be
    // .........................................................................
    
    assertNotEqual(0, idx.id);                
    assertEqual("bitarray", idx.type);        
    assertEqual(false, idx.unique);
    assertEqual([["a", [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, [ ]]]], idx.fields);
    assertEqual(true, idx.isNewlyCreated);

    
    // .........................................................................
    // Attempt to create the index again this time we are going to change 
    // the list of values 
	// The database should pick up the existing bitarray index with the 
	// original attribute/values.
    // .........................................................................

    idx = collection.ensureBitarray("a",["a","b","c","d",0,1,2,3,4,5,6,7,8,9,[]]);
    
    assertNotEqual(0, idx.id);                
    assertEqual("bitarray", idx.type);        
    assertEqual(false, idx.unique);
    assertEqual([["a", [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, [ ]]]], idx.fields);
    assertEqual(false, idx.isNewlyCreated);
    
    return;
  },

  
  testCreationBitarrayIndex_2b : function () {
    var idx;
    var result;
    
    // .........................................................................
    // Create a bit array index with one attribute
    // .........................................................................
    
    idx = collection.ensureBitarray("a",[0,1,2,3,4,5,6,7,8,9,[]]);
    
    
    // .........................................................................
    // Got through the returned json object and check whether things are as 
    // they should be
    // .........................................................................
    
    assertNotEqual(0, idx.id);                
    assertEqual("bitarray", idx.type);        
    assertEqual(false, idx.unique);
    assertEqual([["a", [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, [ ]]]], idx.fields);
    assertEqual(true, idx.isNewlyCreated);

    
	// .........................................................................
	// Remove the index
    // .........................................................................
	
    result = collection.dropIndex(idx.id);         
    assertEqual(true,result);
    
    // .........................................................................
    // Attempt to create the index again this time we are going to change 
    // the list of values.
    // This time the index should show the new attribute/values	
    // .........................................................................

    idx = collection.ensureBitarray("a",["a","b","c","d",0,1,2,3,4,5,6,7,8,9,[]]);
    
    assertNotEqual(0, idx.id);                
    assertEqual("bitarray", idx.type);        
    assertEqual(false, idx.unique);
    assertEqual([["a", ["a", "b", "c", "d", 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, [ ]]]], idx.fields);
    assertEqual(true, idx.isNewlyCreated);
    
    return;
  },

  
  testCreationBitarrayIndex_2c : function () {
    var idx;
       
    // .........................................................................
    // Create a bit array index with one attribute, with different types of
	// json objects and with 3 attributes and their corresponding values.
    // .........................................................................
    
    idx = collection.ensureBitarray("x",["male","female","other"],"y",[0,1,3,[]],"z",[{"z1":0},{"z2":1},[[["hello","goodbye"]],0,1,2,3,["4","4a","4b"],5,6,7,8,9],[]]);
    
    
    // .........................................................................
    // Got through the returned json object and check whether things are as 
    // they should be
    // .........................................................................
    assertNotEqual(0, idx.id);                
    assertEqual("bitarray", idx.type);        
    assertEqual(false, idx.unique);
    assertEqual([["x", ["male", "female", "other"]], ["y", [0, 1, 3, [ ]]], ["z", [{ "z1" : 0 }, { "z2" : 1 }, [[["hello","goodbye"]],0, 1, 2, 3, ["4","4a","4b"], 5, 6, 7, 8, 9], [ ]]]], idx.fields);
    assertEqual(true, idx.isNewlyCreated);
    
    return;
  },


  
////////////////////////////////////////////////////////////////////////////////
/// @brief test: bitarray index insertion
////////////////////////////////////////////////////////////////////////////////

  testCreationBitarrayIndex_3a : function () {
    var idx;
    var n = 10000;   
    
    // .........................................................................
    // Create a bit array index with one attribute possible values are 0-9
    // inclusive.
    // .........................................................................
    
    idx = collection.ensureBitarray("x",[0,1,2,3,4,5,6,7,8,9]);
    
    /* print("Start writing",n," documents into index"); */
    
    // .........................................................................
    // Save n random records. All of these records should be inserted into the
    // collection.
    // .........................................................................
    
    for (var j = 0; j < n; ++j) {
      var value = Math.floor(Math.random()*10);
      collection.save({"x":value});      
    }
    
    /* print("End writing",n," documents into index"); */
    
    return;
  },

  testCreationBitarrayIndex_3b : function () {
    var idx;
    var n = 10000;   
	
    // .........................................................................
    // Create a bit array index with one attribute
    // .........................................................................
    
    idx = collection.ensureBitarray("x",[0,1,2,3,4,5,6,7,8,9,[]]);
    
    /* print("Start writing",n," documents into index"); */
	
    // .........................................................................
    // Save n random records. The values of the attribute x will be in the 
    // range 0-19. Later our expections will approx half of these records
    // will be 'other' bitarray column
    // .........................................................................
    
    for (var j = 0; j < n; ++j) {
      var value = Math.floor(Math.random()*20);
      collection.save({"x":value});      
      //print("saved ",j,value);
    }
    
    /* print("End writing",n," documents into index"); */
    
    return;
  },
  

  testCreationBitarrayIndex_3c : function () {
    var idx;
  
    // .........................................................................
    // Create a bit array index with one attribute
    // .........................................................................
    
    idx = collection.ensureBitarray("x",[0,1,2,3,4,5,6,7,8,9]);

    
    // .........................................................................    
    // attempting to save this document should fail
    // .........................................................................    
    
    try {
      collection.save({"x":"hi there!"});      
    }
    catch(err) {
      return;
    }  
    
   
    throw("Bitarray index should not allow the insertion of this document within the collection");
    
    return;
  },
  

  testCreationBitarrayIndex_3d : function () {
    var idx;
    var n = 10000;   
    // .........................................................................
    // Create a bit array index with one attribute
    // .........................................................................
    
    idx = collection.ensureBitarray("x",[0,1,2,3,4,5,6,7,8,9,[]],"y.z",["a","b","c","d","e","f","g","h","i","j",[]],
                                     "a.b.c",["a","b","c"]);
    
    /* print("Start writing",n," documents into index"); */
    
    // .........................................................................
    // Save n random records
    // .........................................................................
    
    for (var j = 0; j < n; ++j) {
      var value  = Math.floor(Math.random()*10);
      var alpha1 = Math.floor(Math.random()*10);
      var alpha2 = Math.floor(Math.random()*3);
      collection.save({"x":value, 
                       "y":{"a":1,"z":String.fromCharCode(97 + alpha1)},
                       "a":{"x":2,"b":{"x":3,"c":String.fromCharCode(97 + alpha2)}}
                       });      
      //print("saved ",j,value,String.fromCharCode(97 + alpha));
    }
    
    /* print("End writing",n," documents into index"); */
    
    return;
  },


  
////////////////////////////////////////////////////////////////////////////////
/// @brief test: bitarray index removal
////////////////////////////////////////////////////////////////////////////////
  
  testCreationBitarrayIndex_4a : function () {
    var idx;
    var n = 10000; 
    var docs = new Array();
    var visited = new Array();
    
    // .........................................................................
    // Create a bit array index with one attribute
    // .........................................................................
    
    idx = collection.ensureBitarray("x",[0,1,2,3,4,5,6,7,8,9,[]],"y.z",["a","b","c","d","e","f","g","h","i","j",[]]);
        
    // .........................................................................
    // Save n random records
    // .........................................................................
    
    // print("Start writing",n," documents into index"); 
    
    for (var j = 0; j < n; ++j) {
      var value = Math.floor(Math.random()*10);
      var alpha = Math.floor(Math.random()*10);
      var doc;
      
      doc = collection.save({"x":value, "y":{"a":1,"z":String.fromCharCode(97 + alpha)} });      
      docs.push(doc._id);
      //print("saved ",j,value,String.fromCharCode(97 + alpha), doc);
    }
        
    // print("End writing",n," documents into index"); 

    
    // .........................................................................
    // Remove n random records
    // .........................................................................
    
    // print("Start removal ",n," documents into index"); 
    
    for (var j = 0; j < n; ++j) {
      var value = Math.floor(Math.random()*n);
      var doc   = docs[value];
      var result;
      //print("a removed ",j,value,doc,visited[doc]);
      if (visited[doc] == undefined) {
        result = collection.remove(doc);      
        visited[doc] = true;
        //print("b removed ",j,value,doc,result);
      }  
    }
    
    // print("End removal ",n," documents into index"); 
    return;
  },
  
  
////////////////////////////////////////////////////////////////////////////////
/// @brief test: bitarray index lookup
////////////////////////////////////////////////////////////////////////////////
  
  testLookupBitarrayIndex_5a : function () {
    var idx;
    var n = 1000; 
    var bitarrayValues = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,[]];
    var shelves = new Array(bitarrayValues.length);
    var docs = new Array();
    var totalDocs = 0;
    var result;

    
    // .........................................................................
    // initialise our counters to 0
    // .........................................................................
    
    for (var j = 0; j < shelves.length; ++j) {
      shelves[j] = 0;
    }
    
    // .........................................................................
    // Create a bit array index with one attribute
    // .........................................................................
    
    idx = collection.ensureBitarray("x",bitarrayValues);
        

    // .........................................................................
    // Save n random records
    // .........................................................................
    
    // print("Start writing",n," documents into index");
        
    for (var j = 0; j < n; ++j) {
      var value = Math.floor(Math.random()*(bitarrayValues.length + 5));
      var alpha = Math.floor(Math.random()*(bitarrayValues.length + 5));
      var doc;
      
      doc = collection.save({"x":value, "y":{"a":1,"z":String.fromCharCode(97 + alpha)} });      
      docs.push(doc._id);
      if (value > bitarrayValues[bitarrayValues.length - 2]) {
        value = bitarrayValues.length - 1;
      }  
      shelves[value] = shelves[value] + 1;
      totalDocs += 1;
    }
        
    // print("End writing",n," documents into index");

    // .........................................................................
    // Ensure that we have the total number of written documents stored in the
    // last position
    // .........................................................................
    
    assertEqual(totalDocs, n);        
    
    // print("Start Lookup");
    // print(shelves);
    for (var j = 0; j < bitarrayValues.length; ++j) {
      result = collection.BY_EXAMPLE_BITARRAY(idx.id,{"x":j,"y":6,"z":-1}, null, null);
      //print(result['total'],shelves[j],j);
      assertEqual(result['total'],shelves[j]);
    }
    
    // print("End Lookup");
    return;
    
  },
  
  
/*  
  testLookupBitarrayIndex_3 : function () {
    var idx;
    var n = 2000; 
    var bitarrayValues = [ [0,1,2,[]], ["a","b",[], "c"] ];
    var maxValues      = [2,2];
    var otherPosition  = [3,2];
    var shelves = new Array(bitarrayValues[0].length + 1);
    var docs = new Array();
    var result;

    
    // .........................................................................
    // initialise our counters to 0
    // .........................................................................
    
    for (var i = 0; i < shelves.length; ++i) {
      shelves[i] = new Array(bitarrayValues[1].length + 1);
      for (var j = 0; j < shelves[i].length; ++j) {
        shelves[i][j] = 0;
      }
    }
    
    // .........................................................................
    // Create a bit array index with two attributes
    // .........................................................................
    
    idx = collection.ensureBitarray("x", bitarrayValues[0], "y", bitarrayValues[1]);
        

    // .........................................................................
    // Save n random records
    // .........................................................................
    
    print("Start writing",n," documents into index");
        
    for (var j = 0; j < n; ++j) {
      var value = Math.floor(Math.random()*(bitarrayValues.length + 5));
      var alpha = Math.floor(Math.random()*(bitarrayValues.length + 5));
      var doc;
      var subArray;
      
      doc = collection.save({"x":value, "y": String.fromCharCode(97 + alpha), "z": {"xx":value,"yy":alpha} });      
      docs.push(doc._id);
      print("j=",j,"value=",value,"alpha=",alpha);
      //var bitarrayValues = [ [0,1,2,[]], ["a","b",[], "c"] ];
      
      if (value > maxValues[0]) {
        subArray = shelves[otherPosition[0]];
      }
      else {
        if (value < otherPosition[0]) {
          subArray = shelves[value];
        }
        else {
          subArray = shelves[value + 1];
        }        
      }        
      
      if (alpha > maxValues[1]) {
        subArray[otherPosition[1]] += 1;
      }
      else {
        if (alpha < otherPosition[1]) {
          subArray[alpha] += 1;
        }
        else {
          subArray[alpha + 1] += 1;
        }
      }
      subArray[subArray.length - 1] += 1;
      
      //print("j=",j,"value=",value,"alpha=",alpha,subArray);
    }
        
    print("End writing",n," documents into index");

    for (var j = 0; j < (shelves[shelves.length - 1].length) ; ++j) {
      for (var i = 0; i < (shelves.length - 1) ; ++i) {
        shelves[shelves.length - 1][j] += shelves[i][j];
      }  
    }


    for (var i = 0; i < shelves.length ; ++i) {
      for (var j = 0; j < shelves[i].length; ++j) {
        //print(shelves[i][j]);
      }      
    }

    // .........................................................................
    // Ensure that we have the total number of written documents stored in the
    // last position
    // .........................................................................
    
    assertEqual(shelves[shelves.length - 1][shelves[shelves.length - 1].length - 1], n);        
    
    
    
    print("Start Lookup");
    print(shelves);
    for (var i = 0; i < bitarrayValues[0].length; ++i) {
      for (var j = 0; j < bitarrayValues[1].length; ++j) {
        var subArray;
        var subSubArray;
        
        result = collection.BY_EXAMPLE_BITARRAY(idx.id,{"x":i,"y":String.fromCharCode(97 + j)}, null, null);
        
        
        if (i > maxValues[0]) {
          subArray = shelves[otherPosition[0]];
        }
        else{
          if (i < otherPosition[0]) {
            subArray = shelves[i];
          }
          else {
            subArray = shelves[i + 1];
          }
        }
        
        
        if (j > maxValues[1]) {
          subSubArray = subArray[otherPosition[1]];
        }
        else {        
          if (j < otherPosition[1]) {
            subSubArray = subArray[j];
          }
          else {
            subSubArray = subArray[j + 1];
          }
        }  
        print(result['total'],subSubArray,i,j);
        assertEqual(result['total'],subSubArray);
      }
    }
    print("End Lookup");
    return;
    
  },
  
  
  
  
  
  // ...........................................................................
  // In this test we have not specified a bitarray column for values outside
  // the range 0-9.
  // ...........................................................................
  
  testLookupBitarrayIndex_2 : function () {
    var idx;
    var n = 100; 
    var bitarrayValues = [0,1,2,3,4,5,6,7,8,9];
    var shelves = new Array(bitarrayValues.length + 2);
    var docs = new Array();
    var result;

    
    // .........................................................................
    // initialise our counters to 0
    // .........................................................................
    
    for (var j = 0; j < shelves.length; ++j) {
      shelves[j] = 0;
    }
    
    // .........................................................................
    // Create a bit array index with one attribute
    // .........................................................................
    
    idx = collection.ensureBitarray("x",bitarrayValues);
        

    // .........................................................................
    // Save n random records
    // .........................................................................
    
    print("Start writing",n," documents into index");
        
    for (var j = 0; j < n; ++j) {
      var value = Math.floor(Math.random()*(bitarrayValues.length + 5));
      var alpha = Math.floor(Math.random()*(bitarrayValues.length + 5));
      var doc;
      
      doc = collection.save({"x":value, "y":{"a":1,"z":String.fromCharCode(97 + alpha)} });      
      docs.push(doc._id);
      if (value > bitarrayValues[bitarrayValues.length - 1]) {
        shelves[shelves.length - 2] = shelves[shelves.length - 2] + 1;
      }
      else {
        shelves[value] = shelves[value] + 1;
      }        
    }
        
    print("End writing",n," documents into index");

    for (var j = 0; j < (shelves.length - 1) ; ++j) {
      shelves[shelves.length - 1] = shelves[shelves.length - 1] + shelves[j];
    }
    
    // .........................................................................
    // Ensure that we have the total number of written documents stored in the
    // last position
    // .........................................................................
    
    assertEqual(shelves[shelves.length - 1], n);        
    
    
    print("Start Lookup");
    print(shelves);
    for (var j = 0; j < bitarrayValues.length; ++j) {
      result = collection.BY_EXAMPLE_BITARRAY(idx.id,{"x":j,"y":6,"z":-1}, null, null);
      print(result['total'],shelves[j]);
      assertEqual(result['total'],shelves[j]);
    }

    result = collection.BY_EXAMPLE_BITARRAY(idx.id,{"x":"hi there"}, null, null);
    assertEqual(result['total'],shelves[bitarrayValues.length]);
    
    print("End Lookup");
    return;
  },
  
  


    
  


  
  
  
  
  
  */
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(BitarrayIndexSuite);
return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
