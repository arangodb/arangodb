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

var arangodb = require("org/arangodb");
var internal = require("internal");
var db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TaskSuite () {
  var cn = "UnitTestsTasks";

  var cleanTasks = function () {
    internal.getTasks().forEach(function(task) {
      if (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/)) {
        internal.deleteTask(task);
      }
    });
  };
      
  var getTasks = function () {
    var sorter = function (l, r) {
      if (l.id !== r.id) {
        return (l.id < r.id ? -1 : 1);
      }
      return 0;
    };

    return internal.getTasks().filter(function (task) {
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
        internal.executeTask({ });
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task without a period
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskNoPeriod : function () {
      try {
        internal.executeTask({ id: "UnitTestsNoPeriod", command: "1+1;" });
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
        internal.executeTask({ id: "UnitTestsNoPeriod", period: -1, command: "1+1;" });
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
        internal.executeTask({ id: "UnitTestsNoPeriod", period: 0, command: "1+1;" });
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
        internal.executeTask({ id: "UnitTestsNoPeriod", period: 1 });
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
      var task = internal.executeTask({ name: "UnitTests1", command: "1+1;", period: 1 });

      assertMatch(/^\d+$/, task.id);
      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task with a duplicate id
////////////////////////////////////////////////////////////////////////////////

    testCreateTaskDuplicateId : function () {
      var task = internal.executeTask({ id: "UnitTests1", name: "UnitTests1", command: "1+1;", period: 1 });

      assertEqual("UnitTests1", task.id);
      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);

      assertEqual(1, task.period);

      try {
        internal.executeTask({ id: "UnitTests1", name: "UnitTests1", command: "1+1;", period: 1 });
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
        internal.deleteTask();
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
      var task = internal.executeTask({ id: "UnitTests1", name: "UnitTests1", command: "1+1;", period: 1 });

      assertEqual("UnitTests1", task.id);
      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);

      internal.deleteTask(task);

      try {
        // deleting again should fail
        internal.deleteTask(task);
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
      var task = internal.executeTask({ id: "UnitTests1", name: "UnitTests1", command: "1+1;", period: 1 });

      assertEqual("UnitTests1", task.id);
      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);

      internal.deleteTask(task.id);

      try {
        // deleting again should fail
        internal.deleteTask(task.id);
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_TASK_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of tasks
////////////////////////////////////////////////////////////////////////////////

    testGetTasks : function () {
      var task1 = internal.executeTask({ 
        id: "UnitTests1", 
        name: "UnitTests1", 
        command: "1+1;",
        period: 1
      });

      var task2 = internal.executeTask({ 
        id: "UnitTests2", 
        name: "UnitTests2", 
        command: "2+2;",
        period: 2
      });

      var tasks = getTasks();

      assertEqual(2, tasks.length);
      assertEqual(task1.id, tasks[0].id);
      assertEqual(task1.name, tasks[0].name);
      assertEqual(task1.type, tasks[0].type);
      assertEqual(task1.period, tasks[0].period);
      assertEqual(task1.database, tasks[0].database);

      assertEqual(task2.id, tasks[1].id);
      assertEqual(task2.name, tasks[1].name);
      assertEqual(task2.type, tasks[1].type);
      assertEqual(task2.period, tasks[1].period);
      assertEqual(task2.database, tasks[1].database);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of tasks pre and post task deletion
////////////////////////////////////////////////////////////////////////////////

    testGetTasks : function () {
      var task1 = internal.executeTask({ 
        id: "UnitTests1", 
        name: "UnitTests1", 
        command: "1+1;",
        period: 1
      });

      var tasks = getTasks();

      assertEqual(1, tasks.length);
      assertEqual(task1.id, tasks[0].id);
      assertEqual(task1.name, tasks[0].name);
      assertEqual(task1.type, tasks[0].type);
      assertEqual(task1.period, tasks[0].period);
      assertEqual(task1.database, tasks[0].database);

      var task2 = internal.executeTask({ 
        id: "UnitTests2", 
        name: "UnitTests2", 
        command: "2+2;",
        period: 2
      });

      tasks = getTasks();

      assertEqual(2, tasks.length);
      assertEqual(task1.id, tasks[0].id);
      assertEqual(task1.name, tasks[0].name);
      assertEqual(task1.type, tasks[0].type);
      assertEqual(task1.period, tasks[0].period);
      assertEqual(task1.database, tasks[0].database);
      assertEqual(task2.id, tasks[1].id);
      assertEqual(task2.name, tasks[1].name);
      assertEqual(task2.type, tasks[1].type);
      assertEqual(task2.period, tasks[1].period);
      assertEqual(task2.database, tasks[1].database);

      internal.deleteTask(task1);
      
      tasks = getTasks();

      assertEqual(1, tasks.length);
      assertEqual(task2.id, tasks[0].id);
      assertEqual(task2.name, tasks[0].name);
      assertEqual(task2.type, tasks[0].type);
      assertEqual(task2.period, tasks[0].period);
      assertEqual(task2.database, tasks[0].database);

      internal.deleteTask(task2);
      
      tasks = getTasks();

      assertEqual(0, tasks.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a task and run it
////////////////////////////////////////////////////////////////////////////////

    testCreateStringCommand : function () {
      db._drop(cn);
      db._create(cn);

      assertEqual(0, db[cn].count());

      var command = "require('internal').db." + cn + ".save({ value: params });";

      var task = internal.executeTask({ 
        id: "UnitTests1", 
        name: "UnitTests1", 
        command: command, 
        period: 1,
        params: 23 
      });

      assertEqual("UnitTests1", task.id);
      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);
      assertEqual("_system", task.database);

      internal.wait(5);

      internal.deleteTask(task);

      assertTrue(db[cn].count() > 0);
      assertTrue(db[cn].byExample({ value: 23 }).count() > 0);
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

      var task = internal.executeTask({ 
        id: "UnitTests1", 
        name: "UnitTests1", 
        command: command, 
        period: 1,
        params: { cn: cn, val: 42 } 
      });
      
      assertEqual("UnitTests1", task.id);
      assertEqual("UnitTests1", task.name);
      assertEqual("periodic", task.type);
      assertEqual(1, task.period);
      assertEqual("_system", task.database);

      internal.wait(5);

      internal.deleteTask(task);

      assertTrue(db[cn].count() > 0);
      assertTrue(db[cn].byExample({ value: 23 }).count() === 0);
      assertTrue(db[cn].byExample({ value: 42 }).count() > 0);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(TaskSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
