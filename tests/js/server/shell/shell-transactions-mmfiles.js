/*jshint globalstrict:false, strict:false, maxlen : 200 */
/*global fail, assertTrue, assertEqual, TRANSACTION */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for transactions
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const arangodb = require("@arangodb");
const db = arangodb.db;
const testHelper = require("@arangodb/test-helper").Helper;
const isCluster = require('@arangodb/cluster').isCluster();

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function transactionServerFailuresSuite () {
  'use strict';
  var cn = "UnitTestsTransaction";

  var c = null;
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.debugClearFailAt();
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.debugClearFailAt();

      if (c !== null) {
        c.drop();
      }

      c = null;
      internal.wait(0);
    },
  
////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of starting a transaction fails
////////////////////////////////////////////////////////////////////////////////

    testBeginTransactionFailure : function () {
      internal.debugClearFailAt();
      db._drop(cn);
      c = db._create(cn);

      internal.debugSetFailAt("LogfileManagerRegisterTransactionOom");

      try {
        db._executeTransaction({ 
          collections: {
            write: cn 
          },
          action: function () {
            c.save({ value: 1 });
            fail();
          }
        });

        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertUniqueHashIndexServerFailures : function () {
      var failures = [ "InsertPrimaryIndex", "InsertSecondaryIndexes", "InsertHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        c.ensureUniqueConstraint("value");

        internal.debugSetFailAt(f);

        try {
          c.save({ value: 1 });
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(0, c.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertUniqueHashIndexServerFailuresTrx : function () {
      var failures = [ "InsertPrimaryIndex", "InsertSecondaryIndexes", "InsertHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        c.ensureUniqueConstraint("value");

        db._executeTransaction({ 
          collections: {
            write: cn 
          },
          action: function () {
            c.save({ value: 1 });

            internal.debugSetFailAt(f);
            try {
              c.save({ value: 2 });
              fail();
            }
            catch (err) {
              assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
            }

            assertEqual(1, c.count());
            internal.debugClearFailAt();
          }
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertUniqueHashIndexServerFailuresRollback : function () {
      var failures = [ "InsertPrimaryIndex", "InsertSecondaryIndexes", "InsertHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        c.ensureUniqueConstraint("value");

        try {
          db._executeTransaction({ 
            collections: {
              write: cn 
            },
            action: function () {
              c.save({ value: 1 });

              internal.debugSetFailAt(f);
              c.save({ value: 2 }); 
            }
          });
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(0, c.count());
        internal.debugClearFailAt();
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertNonUniqueHashIndexServerFailures : function () {
      var failures = [ "InsertPrimaryIndex", "InsertSecondaryIndexes", "InsertHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        c.ensureHashIndex("value");

        internal.debugSetFailAt(f);

        try {
          c.save({ value: 1 });
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(0, c.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertNonUniqueHashIndexServerFailuresTrx : function () {
      var failures = [ "InsertPrimaryIndex", "InsertSecondaryIndexes", "InsertHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        c.ensureHashIndex("value");

        db._executeTransaction({ 
          collections: {
            write: cn 
          },
          action: function () {
            c.save({ value: 1 });

            internal.debugSetFailAt(f);
            try {
              c.save({ value: 2 });
              fail();
            }
            catch (err) {
              assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
            }

            assertEqual(1, c.count());
            internal.debugClearFailAt();
          }
        });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertNonUniqueHashIndexServerFailuresRollback : function () {
      var failures = [ "InsertPrimaryIndex", "InsertSecondaryIndexes", "InsertHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        c.ensureHashIndex("value");

        try {
          db._executeTransaction({ 
            collections: {
              write: cn 
            },
            action: function () {
              c.save({ value: 1 });

              internal.debugSetFailAt(f);
              c.save({ value: 2 }); 
            }
          });
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(0, c.count());
        internal.debugClearFailAt();
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveUniqueHashIndexServerFailures : function () {
      var failures = [ "DeletePrimaryIndex", "DeleteSecondaryIndexes", "RemoveHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        for (var i = 0; i < 1000; ++i) {
          c.save({ value: i });
        }
        c.ensureUniqueConstraint("value");

        internal.debugSetFailAt(f);
      
        try {
          c.truncate();
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(1000, c.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveNonUniqueHashIndexServerFailures : function () {
      var failures = [ "DeletePrimaryIndex", "DeleteSecondaryIndexes", "RemoveHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        for (var i = 0; i < 1000; ++i) {
          c.save({ value: i % 10 });
        }
        c.ensureHashIndex("value");

        internal.debugSetFailAt(f);
      
        try {
          c.truncate();
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(1000, c.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveNonUniqueHashIndexServerFailuresTrx : function () {
      var failures = [ "DeletePrimaryIndex", "DeleteSecondaryIndexes", "RemoveHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        c.ensureHashIndex("value");
        for (var i = 0; i < 1000; ++i) {
          c.save({ _key: "test" + i, value: i % 10 });
        }

        db._executeTransaction({ 
          collections: {
            write: cn 
          },
          action: function () {
            for (var j = 0; j < 10; ++j) {
              c.remove("test" + j);
            }
 
            internal.debugSetFailAt(f);

            try {
              c.remove("test10");
              fail();
            }
            catch (err) {
              assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
            }
          }
        });

        assertEqual(990, c.count());
        internal.debugClearFailAt();
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveNonUniqueHashIndexServerFailuresRollback : function () {
      var failures = [ "DeletePrimaryIndex", "DeleteSecondaryIndexes", "RemoveHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        for (var i = 0; i < 1000; ++i) {
          c.save({ _key: "test" + i, value: i % 10 });
        }

        c.ensureHashIndex("value");

        try {
          db._executeTransaction({ 
            collections: {
              write: cn 
            },
            action: function () {
              for (var j = 0; j < 10; ++j) {
                c.remove("test" + j);
              }  

              internal.debugSetFailAt(f);
              c.remove("test10");
            }
          });
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(1000, c.count());
        internal.debugClearFailAt();
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveUniqueSkiplistServerFailures : function () {
      var failures = [ "DeletePrimaryIndex", "DeleteSecondaryIndexes" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        for (var i = 0; i < 1000; ++i) {
          c.save({ value: i });
        }
        c.ensureUniqueSkiplist("value");

        internal.debugSetFailAt(f);
      
        try {
          c.truncate();
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(1000, c.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveNonUniqueSkiplistServerFailures : function () {
      var failures = [ "DeletePrimaryIndex", "DeleteSecondaryIndexes" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        for (var i = 0; i < 1000; ++i) {
          c.save({ value: i % 10 });
        }
        c.ensureSkiplist("value");

        internal.debugSetFailAt(f);
      
        try {
          c.truncate();
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(1000, c.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveMultipleIndexesServerFailures : function () {
      var failures = [ "DeletePrimaryIndex", "DeleteSecondaryIndexes", "RemoveHashIndex" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        for (var i = 0; i < 1000; ++i) {
          c.save({ value: i % 10 });
        }
        c.ensureSkiplist("value");
        c.ensureHashIndex("value");

        internal.debugSetFailAt(f);
      
        try {
          c.truncate();
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(1000, c.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testReadServerFailures : function () {
      var failures = [ "ReadDocumentNoLock", "ReadDocumentNoLockExcept" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        internal.debugSetFailAt(f);
      
        try {
          c.document("foo");
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertServerFailuresEmpty : function () {
      var failures = [ 
                       "InsertDocumentNoHeader",
                       "InsertDocumentNoHeaderExcept",
                       "InsertDocumentNoLock",
                       "InsertDocumentNoOperation",
                       "InsertDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept",
                       "TransactionOperationAfterAdjust",
                       "TransactionOperationAtEnd" ]; 

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        internal.debugSetFailAt(f);
      
        try {
          c.save({ _key: "foo", a: 1 });
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, f);
        }

        assertEqual(0, c.count());
      });
    },
   
////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertServerFailuresNonEmpty : function () {
      var failures = [ 
                       "InsertDocumentNoHeader",
                       "InsertDocumentNoHeaderExcept",
                       "InsertDocumentNoLock",
                       "InsertDocumentNoOperation",
                       "InsertDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept",
                       "TransactionOperationAfterAdjust",
                       "TransactionOperationAtEnd" ]; 

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);
        c.save({ _key: "bar", foo: "bar" });
        assertEqual(1, c.count());
        assertEqual("bar", c.document("bar").foo);

        internal.debugSetFailAt(f);
      
        try {
          c.save({ _key: "foo", a: 1 });
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, f);
        }

        assertEqual(1, c.count());
        assertEqual("bar", c.document("bar").foo);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertServerFailuresConstraint : function () {
      var failures = [ 
                       "InsertDocumentNoHeader",
                       "InsertDocumentNoHeaderExcept",
                       "InsertDocumentNoLock" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);
        c.save({ _key: "foo", foo: "bar" });
        assertEqual(1, c.count());
        assertEqual("bar", c.document("foo").foo);

        internal.debugSetFailAt(f);
      
        try {
          c.save({ _key: "foo", foo: "baz" });
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, f);
        }

        assertEqual(1, c.count());
        assertEqual("bar", c.document("foo").foo);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testInsertServerFailuresMulti : function () {
      var failures = [ 
                       "InsertDocumentNoHeader",
                       "InsertDocumentNoHeaderExcept",
                       "InsertDocumentNoLock",
                       "InsertDocumentNoOperation",
                       "InsertDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept",
                       "TransactionOperationAfterAdjust",
                       "TransactionOperationPushBack",
                       "TransactionOperationPushBack2",
                       "TransactionOperationAtEnd" ]; 

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        try {
          TRANSACTION({ 
            collections: {
              write: [ cn ],
            },
            action: function () {
              for (var i = 0; i < 10; ++i) {
                if (i === 9) {
                  internal.debugSetFailAt(f);
                }
                c.save({ _key: "test" + i, a: 1 });
              }
            }
          });
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, f);
        }

        assertEqual(0, c.count(), f);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveServerFailuresEmpty : function () {
      var failures = [ "RemoveDocumentNoMarker", "RemoveDocumentNoMarkerExcept", "RemoveDocumentNoLock" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        internal.debugSetFailAt(f);
      
        try {
          c.remove("foo");
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(0, c.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveServerFailuresNonEmpty : function () {
      var failures = [ "RemoveDocumentNoMarker",
                       "RemoveDocumentNoMarkerExcept",
                       "RemoveDocumentNoLock",
                       "RemoveDocumentNoOperation",
                       "RemoveDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept",
                       "TransactionOperationAfterAdjust",
                       "TransactionOperationAtEnd" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);
        c.save({ _key: "foo", foo: "bar" });
        assertEqual(1, c.count());
        assertEqual("bar", c.document("foo").foo);

        internal.debugSetFailAt(f);
      
        try {
          c.remove("foo");
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(1, c.count());
        assertEqual("bar", c.document("foo").foo);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testRemoveServerFailuresMulti : function () {
      var failures = [ "RemoveDocumentNoMarker",
                       "RemoveDocumentNoMarkerExcept",
                       "RemoveDocumentNoLock",
                       "RemoveDocumentNoOperation",
                       "RemoveDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept",
                       "TransactionOperationAfterAdjust",
                       "TransactionOperationPushBack",
                       "TransactionOperationPushBack2",
                       "TransactionOperationAtEnd" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);
        var i;
        for (i = 0; i < 10; ++i) {
          c.save({ _key: "test" + i, a: i });
        }

        try {
          TRANSACTION({ 
            collections: {
              write: [ cn ],
            },
            action: function () {
              for (var i = 0; i < 10; ++i) {
                if (i === 9) {
                  internal.debugSetFailAt(f);
                }
                c.remove("test" + i);
              }
            }
          });
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(10, c.count());
        for (i = 0; i < 10; ++i) {
          assertEqual(i, c.document("test" + i).a);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testUpdateServerFailuresNonEmpty : function () {
      var failures = [ 
                       "UpdateDocumentNoMarker",
                       "UpdateDocumentNoMarkerExcept",
                       "UpdateDocumentNoLock",
                       "UpdateDocumentNoOperation",
                       "UpdateDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept",
                       "TransactionOperationAfterAdjust",
                       "TransactionOperationAtEnd" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);
        c.save({ _key: "foo", foo: "bar" });
        assertEqual(1, c.count());
        assertEqual("bar", c.document("foo").foo);

        internal.debugSetFailAt(f);
      
        try {
          c.update("foo", { bar: "baz" });
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, f);
        }

        assertEqual(1, c.count());
        assertEqual("bar", c.document("foo").foo);
        assertEqual(undefined, c.document("foo").bar);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testUpdateServerFailuresMulti : function () {
      var failures = [ 
                       "UpdateDocumentNoMarker",
                       "UpdateDocumentNoMarkerExcept",
                       "UpdateDocumentNoLock",
                       "UpdateDocumentNoOperation",
                       "UpdateDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept",
                       "TransactionOperationAfterAdjust",
                       "TransactionOperationPushBack",
                       "TransactionOperationPushBack2",
                       "TransactionOperationAtEnd" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);
        var i;
        for (i = 0; i < 10; ++i) {
          c.save({ _key: "test" + i, a: i });
        } 
        assertEqual(10, c.count());

        try {
          TRANSACTION({ 
            collections: {
              write: [ cn ],
            },
            action: function () {
              for (var i = 0; i < 10; ++i) {
                if (i === 9) {
                  internal.debugSetFailAt(f);
                }
                c.update("test" + i, { a: i + 1 });
              }
            }
          });
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, f);
        }

        assertEqual(10, c.count());
        for (i = 0; i < 10; ++i) {
          assertEqual(i, c.document("test" + i).a, f);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testUpdateServerFailuresMultiUpdate : function () {
      var failures = [ 
                       "UpdateDocumentNoMarker",
                       "UpdateDocumentNoMarkerExcept",
                       "UpdateDocumentNoLock",
                       "UpdateDocumentNoOperation",
                       "UpdateDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept",
                       "TransactionOperationAfterAdjust",
                       "TransactionOperationPushBack",
                       "TransactionOperationPushBack2",
                       "TransactionOperationAtEnd" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);
        var i;
        for (i = 0; i < 10; ++i) {
          c.save({ _key: "test" + i, a: i });
        } 
        assertEqual(10, c.count());

        try {
          TRANSACTION({ 
            collections: {
              write: [ cn ],
            },
            action: function () {
              for (var i = 0; i < 10; ++i) {
                if (i === 9) {
                  internal.debugSetFailAt(f);
                }
                // double update
                c.update("test" + i, { a: i + 1 });
                c.update("test" + i, { a: i + 2, b: 2 });
              }
            }
          });
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(10, c.count(), f);
        for (i = 0; i < 10; ++i) {
          assertEqual(i, c.document("test" + i).a, f);
          assertEqual(undefined, c.document("test" + i).b, f);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testTruncateServerFailures : function () {
      var failures = [ "RemoveDocumentNoMarker",
                       "RemoveDocumentNoMarkerExcept",
                       "RemoveDocumentNoLock",
                       "RemoveDocumentNoOperation",
                       "RemoveDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept" ];

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);

        var i;
        for (i = 0; i < 10; ++i) {
          c.save({ _key: "test" + i, a: i });
        }

        internal.debugSetFailAt(f);
      
        try {
          c.truncate();
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        }

        assertEqual(10, c.count());
        for (i = 0; i < 10; ++i) {
          assertEqual(i, c.document("test" + i).a);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: rollback in case of a server-side fail
////////////////////////////////////////////////////////////////////////////////

    testMixedServerFailures : function () {
      var failures = [ 
                       "UpdateDocumentNoMarker",
                       "UpdateDocumentNoMarkerExcept",
                       "UpdateDocumentNoLock",
                       "UpdateDocumentNoOperation",
                       "UpdateDocumentNoOperationExcept",
                       "RemoveDocumentNoMarker",
                       "RemoveDocumentNoMarkerExcept",
                       "RemoveDocumentNoLock",
                       "RemoveDocumentNoOperation",
                       "RemoveDocumentNoOperationExcept",
                       "InsertDocumentNoHeader",
                       "InsertDocumentNoHeaderExcept",
                       "InsertDocumentNoLock",
                       "InsertDocumentNoOperation",
                       "InsertDocumentNoOperationExcept",
                       "TransactionOperationNoSlot",
                       "TransactionOperationNoSlotExcept",
                       "TransactionOperationAfterAdjust",
                       "TransactionOperationPushBack",
                       "TransactionOperationPushBack2",
                       "TransactionOperationAtEnd" ]; 

      failures.forEach (function (f) {
        internal.debugClearFailAt();
        db._drop(cn);
        c = db._create(cn);
        var i;
        for (i = 0; i < 100; ++i) {
          c.save({ _key: "test" + i, a: i });
        } 
        assertEqual(100, c.count());
        
        internal.debugSetFailAt(f);

        try {
          TRANSACTION({ 
            collections: {
              write: [ cn ],
            },
            action: function () {
              var i;
              for (i = 100; i < 150; ++i) {
                c.save({ _key: "test" + i, a: i });
              }
              assertEqual(150, c.count());

              for (i = 0; i < 50; ++i) {
                c.remove("test" + i);
              }
              assertEqual(100, c.count());

              for (i = 50; i < 100; ++i) {
                c.update("test" + i, { a: i - 50, b: "foo" });
              }
              assertEqual(100, c.count());
            }
          });
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum, f);
        }

        assertEqual(100, c.count());
        for (i = 0; i < 100; ++i) {
          assertEqual(i, c.document("test" + i).a, f);
          assertEqual(undefined, c.document("test" + i).b, f);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test disk full during collection
////////////////////////////////////////////////////////////////////////////////

    testDiskFullWhenCollectingTransaction : function () {
      internal.debugClearFailAt();
      if (isCluster) {
        return; // skip
      }

      db._drop(cn);
      c = db._create(cn);
      
      // should not cause any problems yet, but later
      internal.debugSetFailAt("CreateJournalDocumentCollection");
      
      // adjust the configuration and make sure we flush all reserve logfiles
      internal.wal.properties({ reserveLogfiles: 1 });
      
      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      assertEqual(100, c.count());

      for (i = 0; i < 4; ++i) {
        // write something into the logs so we can flush 'em
        c.save({ _key: "foo" });
        c.remove("foo");
        internal.wal.flush(true, false);
      }
       
      // one more to populate a new logfile 
      c.save({ _key: "foo" });
      c.remove("foo");
      
      TRANSACTION({ 
        collections: {
          write: [ cn ],
        },
        action: function () {
          var i;
          for (i = 100; i < 150; ++i) {
            c.save({ _key: "test" + i, a: i });
          }
          assertEqual(150, c.count());

          // make sure we fill up the logfile
          for (i = 0; i < 100000; ++i) {
            c.save({ _key: "foo" + i, value: "the quick brown foxx jumped over the lazy dog" });
          }
        }
      });

      assertEqual(100150, c.count());
      var fig = c.figures();
      assertEqual(100160, fig.uncollectedLogfileEntries);

      internal.debugClearFailAt();
      while (true) {
        try {
          internal.wal.flush(true, true);
          break;
        }
        catch (err) {
          internal.wait(0.5, false);
        }
      }

      assertEqual(100150, c.count());

      testHelper.waitUnload(c);
      
      // data should be there after unload
      assertEqual(100150, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: disk full during transaction
////////////////////////////////////////////////////////////////////////////////

    testDiskFullDuringTransaction : function () {
      internal.debugClearFailAt();
      if (isCluster) {
        return; // takes way too long
      }

      db._drop(cn);
      c = db._create(cn);
      
      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      assertEqual(100, c.count());

      internal.wal.flush(true, true);
        
      try {
        TRANSACTION({ 
          collections: {
            write: [ cn ],
          },
          action: function () {
            var i;
            for (i = 100; i < 150; ++i) {
              c.save({ _key: "test" + i, a: i });
            }
            assertEqual(150, c.count());

            // should not cause any problems
            internal.debugSetFailAt("LogfileManagerGetWriteableLogfile");

            for (i = 0; i < 500000; ++i) {
              c.save({ _key: "foo" + i, value: "the quick brown foxx jumped over the lazy dog" });
            }

            fail();
          }
        });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_NO_JOURNAL.code, err.errorNum);
      }

      assertEqual(100, c.count());
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cannot write begin marker for trx
////////////////////////////////////////////////////////////////////////////////

    testNoBeginMarker : function () {
      internal.debugClearFailAt();

      db._drop(cn);
      c = db._create(cn);
      
      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      assertEqual(100, c.count());

      internal.wal.flush(true, true);
      internal.debugSetFailAt("TransactionWriteBeginMarker");
        
      try {
        TRANSACTION({ 
          collections: {
            write: [ cn ],
          },
          action: function () {
            c.save({ _key: "test100" });
            fail();
          }
        });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
      }

      assertEqual(100, c.count());
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cannot write commit marker for trx
////////////////////////////////////////////////////////////////////////////////

    testNoCommitMarker : function () {
      internal.debugClearFailAt();
      if (isCluster) {
        return; // skip
      }

      db._drop(cn);
      c = db._create(cn);
      
      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      assertEqual(100, c.count());

      internal.wal.flush(true, true);
      internal.debugSetFailAt("TransactionWriteCommitMarker");
        
      try {
        TRANSACTION({ 
          collections: {
            write: [ cn ],
          },
          action: function () {
            var i;
            for (i = 100; i < 1000; ++i) {
              c.save({ _key: "test" + i, a: i });
            }

            assertEqual(1000, c.count());
          }
        });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
      }

      assertEqual(100, c.count());
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cannot write abort marker for trx
////////////////////////////////////////////////////////////////////////////////

    testNoAbortMarker : function () {
      internal.debugClearFailAt();

      db._drop(cn);
      c = db._create(cn);
      
      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      assertEqual(100, c.count());

      internal.wal.flush(true, true);
      internal.debugSetFailAt("TransactionWriteAbortMarker");
        
      try {
        TRANSACTION({ 
          collections: {
            write: [ cn ],
          },
          action: function () {
            var i;
            for (i = 100; i < 1000; ++i) {
              c.save({ _key: "test" + i, a: i });
            }

            assertEqual(1000, c.count());

            throw "rollback!";
          }
        });
      }
      catch (err) {
        // ignore the intentional error
      }
      
      internal.debugClearFailAt();

      testHelper.waitUnload(c);

      assertEqual(100, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cannot write begin marker for trx
////////////////////////////////////////////////////////////////////////////////

    testNoBeginMarkerThrow : function () {
      internal.debugClearFailAt();

      db._drop(cn);
      c = db._create(cn);
      
      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      assertEqual(100, c.count());

      internal.wal.flush(true, true);
      internal.debugSetFailAt("TransactionWriteBeginMarkerThrow");
        
      try {
        TRANSACTION({ 
          collections: {
            write: [ cn ],
          },
          action: function () {
            c.save({ _key: "test100" });
            fail();
          }
        });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_INTERNAL.code, err.errorNum);
      }

      assertEqual(100, c.count());
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cannot write commit marker for trx
////////////////////////////////////////////////////////////////////////////////

    testNoCommitMarkerThrow : function () {
      internal.debugClearFailAt();

      db._drop(cn);
      c = db._create(cn);
      
      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      assertEqual(100, c.count());

      internal.wal.flush(true, true);
      internal.debugSetFailAt("TransactionWriteCommitMarkerThrow");
        
      try {
        TRANSACTION({ 
          collections: {
            write: [ cn ],
          },
          action: function () {
            var i;
            for (i = 100; i < 1000; ++i) {
              c.save({ _key: "test" + i, a: i });
            }

            assertEqual(1000, c.count());
          }
        });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_INTERNAL.code, err.errorNum);
      }

      assertEqual(100, c.count());
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cannot write abort marker for trx
////////////////////////////////////////////////////////////////////////////////

    testNoAbortMarkerThrow : function () {
      internal.debugClearFailAt();

      db._drop(cn);
      c = db._create(cn);
      
      var i;
      for (i = 0; i < 100; ++i) {
        c.save({ _key: "test" + i, a: i });
      } 
      assertEqual(100, c.count());

      internal.wal.flush(true, true);
      internal.debugSetFailAt("TransactionWriteAbortMarkerThrow");
        
      try {
        TRANSACTION({ 
          collections: {
            write: [ cn ],
          },
          action: function () {
            var i;
            for (i = 100; i < 1000; ++i) {
              c.save({ _key: "test" + i, a: i });
            }

            assertEqual(1000, c.count());

            throw "rollback!";
          }
        });
      }
      catch (err) {
        // ignore the intentional error
      }
      
      internal.debugClearFailAt();

      testHelper.waitUnload(c);

      assertEqual(100, c.count());
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

// only run this test suite if server-side failures are enabled
if (internal.debugCanUseFailAt()) {
  jsunity.run(transactionServerFailuresSuite);
}

return jsunity.done();

