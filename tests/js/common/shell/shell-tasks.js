/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, assertMatch, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the task manager
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var internal = require("internal");
var db = arangodb.db;
var tasks = require("@arangodb/tasks");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TaskSuite () {
  var cn = "UnitTestsTasks";

  var cleanTasks = function () {
    tasks.get().forEach(function(task) {
      if (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/)) {
        try {
          tasks.unregister(task);
        }
        catch (err) {
        }
      }
    });
  };

  var getTasks = function (sortAttribute = 'id') {
    var sorter = function (l, r) {
      if (l[sortAttribute] !== r[sortAttribute]) {
        return (l[sortAttribute] < r[sortAttribute] ? -1 : 1);
      }
      return 0;
    };

    return tasks.get().filter(function (task) {
      return task.name.match(/^UnitTest/);
    }).sort(sorter);
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      cleanTasks();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      cleanTasks();

      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task without an id
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskNoId : function () {
      try {
        tasks.register({ });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with an invalid period
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskInvalidPeriod1 : function () {
      try {
        tasks.register({ name: "UnitTestsNoPeriod", period: -1, command: "1+1;" });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with an invalid period
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskInvalidPeriod2 : function () {
      try {
        tasks.register({ name: "UnitTestsNoPeriod", period: 0, command: "1+1;" });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task without a command
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskNoCommand : function () {
      try {
        tasks.register({ name: "UnitTestsNoCommand", period: 1 });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with an automatic id
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskAutomaticId : function () {
      var task = tasks.register({ name: "UnitTestsTaskAutoId", command: "1+1;", period: 1 });

      assertMatch(/^\d+$/, task.id);
      assertEqual("UnitTestsTaskAutoId", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with a duplicate id
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskDuplicateId : function () {
      var task = tasks.register({ id: "UnitTestsTaskDuplicateId", name: "UnitTests1", command: "1+1;", period: 1 });

      assertEqual("UnitTestsTaskDuplicateId", task.id);
      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);

      assertEqual(1, task.period);

      try {
        tasks.register({ id: "UnitTestsTaskDuplicateId", name: "UnitTests2", command: "1+1;", period: 1 });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_TASK_DUPLICATE_ID.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove without an id
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskRemoveWithoutId : function () {
      try {
        tasks.unregister();
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task and remove it
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskRemoveByTask : function () {
      var task = tasks.register({ name: "UnitTests1", command: "1+1;", period: 1 });

      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);

      tasks.unregister(task);

      try {
        // deleting again should fail
        tasks.unregister(task);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_TASK_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task and remove it
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskRemoveById : function () {
      var task = tasks.register({ id: "UnitTests" + Date.now(), name: "UnitTests1", command: "1+1;", period: 1 });

      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);

      tasks.unregister(task.id);

      try {
        // deleting again should fail
        tasks.unregister(task.id);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_TASK_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a single task
////////////////////////////////////////////////////////////////////////////////

    testGetTask : function () {
      var task = tasks.register({
        name: "UnitTests1",
        command: "1+1;",
        period: 1
      });

      var t = tasks.get(task);

      assertEqual(task.id, t.id);
      assertEqual(task.name, t.name);
      assertEqual(task.type, t.type);
      assertEqual(task.period, t.period);
      assertEqual(task.database, t.database);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a single task
////////////////////////////////////////////////////////////////////////////////

    testGetTaskById : function () {
      var task = tasks.register({
        id: "UnitTests" + Date.now(),
        name: "UnitTests1",
        command: "1+1;",
        period: 1
      });

      var t = tasks.get(task.id);

      assertEqual(task.id, t.id);
      assertEqual(task.name, t.name);
      assertEqual(task.type, t.type);
      assertEqual(task.period, t.period);
      assertEqual(task.database, t.database);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of tasks
////////////////////////////////////////////////////////////////////////////////

    testGetTasks : function () {
      var task1 = tasks.register({
        name: "UnitTests1",
        command: "1+1;",
        period: 1
      });

      var task2 = tasks.register({
        name: "UnitTests2",
        command: "2+2;",
        period: 2
      });

      var task3 = tasks.register({
        name: "UnitTests3",
        command: "3+3;",
        offset: 30
      });

      var t = getTasks('name');

      assertEqual(3, t.length);
      assertEqual(task1.id, t[0].id);
      assertEqual(task1.name, t[0].name);
      assertEqual(task1.type, t[0].type);
      assertEqual("periodic", t[0].type);
      assertEqual(task1.period, t[0].period);
      assertEqual(1, t[0].period);
      assertEqual(task1.database, t[0].database);
      assertEqual("_system", t[0].database);

      assertEqual(task2.id, t[1].id);
      assertEqual(task2.name, t[1].name);
      assertEqual(task2.type, t[1].type);
      assertEqual("periodic", t[1].type);
      assertEqual(task2.period, t[1].period);
      assertEqual(2, t[1].period);
      assertEqual(task2.database, t[1].database);
      assertEqual("_system", t[1].database);

      assertEqual(task3.id, t[2].id);
      assertEqual(task3.name, t[2].name);
      assertEqual(task3.type, t[2].type);
      assertEqual("timed", t[2].type);
      assertEqual(30, t[2].offset);
      assertEqual(task3.database, t[2].database);
      assertEqual("_system", t[2].database);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of tasks pre and post task deletion
////////////////////////////////////////////////////////////////////////////////

    testGetTasksPrePost : function () {
      var task1 = tasks.register({
        id: "UnitTests" + Date.now() + "1",
        name: "UnitTests1",
        command: "1+1;",
        period: 1
      });

      var t = getTasks('id');

      assertEqual(1, t.length);
      assertEqual(task1.id, t[0].id);
      assertEqual(task1.name, t[0].name);
      assertEqual(task1.type, t[0].type);
      assertEqual(task1.period, t[0].period);
      assertEqual(task1.database, t[0].database);

      var task2 = tasks.register({
        id: "UnitTests" + Date.now() + "2",
        name: "UnitTests2",
        command: "2+2;",
        period: 2
      });

      t = getTasks('id');

      assertEqual(2, t.length);
      assertEqual(task1.id, t[0].id);
      assertEqual(task1.name, t[0].name);
      assertEqual(task1.type, t[0].type);
      assertEqual(task1.period, t[0].period);
      assertEqual(task1.database, t[0].database);
      assertEqual(task2.id, t[1].id);
      assertEqual(task2.name, t[1].name);
      assertEqual(task2.type, t[1].type);
      assertEqual(task2.period, t[1].period);
      assertEqual(task2.database, t[1].database);

      tasks.unregister(task1);

      t = getTasks('id');

      assertEqual(1, t.length);
      assertEqual(task2.id, t[0].id);
      assertEqual(task2.name, t[0].name);
      assertEqual(task2.type, t[0].type);
      assertEqual(task2.period, t[0].period);
      assertEqual(task2.database, t[0].database);

      tasks.unregister(task2);

      t = getTasks('id');

      assertEqual(0, t.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with an offset and run it
////////////////////////////////////////////////////////////////////////////////

    testOffsetTaskNoExec : function () {
      db._drop(cn);
      db._create(cn);

      assertEqual(0, db[cn].count());

      var command = "require('internal').db." + cn + ".save({ value: params });";

      var task = tasks.register({
        name: "UnitTests1",
        command: command,
        offset: 10,
        params: 42
      });

      assertEqual("UnitTests1", task.name);
      assertEqual("timed", task.type);
      assertEqual(10, task.offset);
      assertEqual("_system", task.database);

      var t = getTasks();
      assertEqual(1, t.length);

      internal.wait(2);

      // task should not have been executed
      assertEqual(0, db[cn].count());
      tasks.unregister(task);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with an offset and run it
////////////////////////////////////////////////////////////////////////////////

    testOffsetTaskExec : function () {
      db._drop(cn);
      db._create(cn);

      assertEqual(0, db[cn].count());

      var command = "require('internal').db." + cn + ".save({ value: params });";

      var task = tasks.register({
        name: "UnitTests1",
        command: command,
        offset: 2,
        params: 42
      });

      assertEqual("UnitTests1", task.name);
      assertEqual("timed", task.type);
      assertEqual(2, task.offset);
      assertEqual("_system", task.database);

      var t = getTasks();
      assertEqual(1, t.length);

      var tries = 0;
      while (tries++ < 15) {
        if (db[cn].count() === 1) {
          return; // alright
        }

        internal.wait(2);
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with an offset and run it
////////////////////////////////////////////////////////////////////////////////

    testOffsetTaskImmediate : function () {
      db._drop(cn);
      db._create(cn);

      assertEqual(0, db[cn].count());

      var command = "require('internal').db." + cn + ".save({ value: params });";

      var task = tasks.register({
        name: "UnitTests1",
        command: command,
        offset: 0,
        params: 42
      });

      assertEqual("UnitTests1", task.name);
      assertEqual("timed", task.type);
      assertEqual(0, Math.round(task.offset, 3));
      assertEqual("_system", task.database);

      var tries = 0;
      while (tries++ < 15) {
        if (db[cn].count() === 1) {
          return; // alright
        }

        internal.wait(2);
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a timer task and run it
////////////////////////////////////////////////////////////////////////////////

    testTimerTask : function () {
      db._drop(cn);
      db._create(cn);

      assertEqual(0, db[cn].count());

      var command = "require('internal').db." + cn + ".save({ value: params });";

      var task = tasks.register({
        name: "UnitTests1",
        command: command,
        offset: 5,
        params: 23
      });

      assertEqual("UnitTests1", task.name);
      assertEqual("timed", task.type);
      assertEqual(5, task.offset);
      assertEqual("_system", task.database);

      internal.wait(5);

      var tries = 0;
      while (tries++ < 15) {
        if (db[cn].count() === 1) {
          return; // alright
        }

        internal.wait(2);
      }

      // task hasn't been executed
      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a timer task and kill it before execution
////////////////////////////////////////////////////////////////////////////////

    testKillTimerTask : function () {
      db._drop(cn);
      db._create(cn);

      assertEqual(0, db[cn].count());

      var command = "require('internal').db." + cn + ".save({ value: params });";

      var task = tasks.register({
        name: "UnitTests1",
        command: command,
        offset: 15,
        params: 23
      });

      assertEqual("UnitTests1", task.name);
      assertEqual("timed", task.type);
      assertEqual(15, task.offset);
      assertEqual("_system", task.database);

      tasks.unregister(task);

      internal.wait(5);

      assertEqual(0, db[cn].count());
      assertEqual(0, db[cn].byExample({ value: 23 }).toArray().length);

      var t = getTasks();
      assertEqual(0, t.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task and run it
////////////////////////////////////////////////////////////////////////////////

    testCreateStringCommand : function () {
      db._drop(cn);
      db._create(cn);

      assertEqual(0, db[cn].count());

      var command = "require('internal').db." + cn + ".save({ value: params });";

      var task = tasks.register({
        name: "UnitTests1",
        command: command,
        period: 1,
        offset: 0,
        params: 17
      });

      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);
      assertEqual("_system", task.database);

      var tries = 0;
      while (tries++ < 15) {
        if (db[cn].count() > 0) {
          assertTrue(db[cn].byExample({ value: 17 }).toArray().length > 0);
          return; // alright
        }

        internal.wait(2);
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task and run it
////////////////////////////////////////////////////////////////////////////////

    testCreateFunctionCommand : function () {
      db._drop(cn);
      db._create(cn);

      assertEqual(0, db[cn].count());

      var command = function (params) {
        require('internal').db[params.cn].save({ value: params.val });
      };

      var task = tasks.register({
        name: "UnitTests1",
        command: command,
        period: 1,
        offset: 0,
        params: { cn: cn, val: 42 }
      });

      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);
      assertEqual("_system", task.database);

      var tries = 0;
      while (tries++ < 15) {
        if (db[cn].count() > 0) {
          assertTrue(db[cn].byExample({ value: 42 }).toArray().length > 0);
          return; // alright
        }

        internal.wait(2);
      }

      fail();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with broken function code
////////////////////////////////////////////////////////////////////////////////

    testBrokenCommand : function () {
      try {
        tasks.register({
          name: "UnitTests1",
          command: "for (i",
          offset: 0
        });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(TaskSuite);

return jsunity.done();
