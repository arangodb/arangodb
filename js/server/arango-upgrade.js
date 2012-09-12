////////////////////////////////////////////////////////////////////////////////
/// @brief arango upgrade actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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

// -----------------------------------------------------------------------------
// --SECTION--                                                    arango-upgrade
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the database
////////////////////////////////////////////////////////////////////////////////

function main (argv) {
  var internal = require("internal");
  var console = require("console");
  var db = internal.db;

  var tasks = [ ];

  // helper function to define tasks
  function addTask (description, code) {
    tasks.push({ description: description, code: code });
  }

  // --------------------------------------------------------------------------
  // the actual upgrade tasks. all tasks defined here should be "re-entrant"
  // --------------------------------------------------------------------------

  // set up the collection _users 
  addTask("setup _users collection", function () {
    var users = db._collection("_users");

    if (users == null) {
      users = db._create("_users", { isSystem: true, waitForSync: true });

      if (users == null) {
        console.error("creating users collection '_users' failed");
        return false;
      }
    }

    return true;
  });

  // create a unique index on username attribute in _users
  addTask("create index on username attribute in _users collection", function () {
    var users = db._collection("_users");

    if (users == null) {
      return false;
    }

    users.ensureUniqueConstraint("username");

    return true;
  });
  
  // add a default root user with no passwd
  addTask("add default root user", function () {
    var users = db._collection("_users");

    if (users == null) {
      return false;
    }

    if (users.count() == 0) {
      // only add account if user has not created his/her own accounts already
      users.save({ user: "root", password: internal.encodePassword(""), active: true });
    }

    return true;
  });
  
  // set up the collection _graphs
  addTask("setup _graphs collection", function () {
    var graphs = db._collection("_graphs");

    if (graphs == null) {
      graphs = db._create("_graphs", { isSystem: true, waitForSync: true });

      if (graphs == null) {
        console.error("creating graphs collection '_graphs' failed");
        return false;
      }
    }

    return true;
  });
  
  // create a unique index on name attribute in _graphs
  addTask("create index on name attribute in _graphs collection", function () {
    var graphs = db._collection("_graphs");

    if (graphs == null) {
      return false;
    }

    graphs.ensureUniqueConstraint("name");

    return true;
  });

  // make distinction between document and edge collections
  addTask("set new collection type for edge collections and update collection version", function () {
    var collections = db._collections();
    
    for (var i in collections) {
      var collection = collections[i];

      try {
        if (collection.version() > 1) {
          // already upgraded
          continue;
        }

        if (collection.type() == 3) {
          // already an edge collection
          collection.setAttribute("version", 2);
          continue;
        }
      
        if (collection.count() > 0) {
          var isEdge = true;
          // check the 1st 50 documents from a collection
          var documents = collection.ALL(0, 50);

          for (var j in documents) {
            var doc = documents[j];
  
            // check if documents contain both _from and _to attributes
            if (! doc.hasOwnProperty("_from") || ! doc.hasOwnProperty("_to")) {
              isEdge = false;
              break;
            }
          }

          if (isEdge) {
            collection.setAttribute("type", 3);
            console.log("made collection '" + collection.name() + " an edge collection");
          }
        }
        collection.setAttribute("version", 2);
      }
      catch (e) {
        console.error("could not upgrade collection '" + collection.name() + "'");
        return false;
      }
    }

    return true;
  });


  console.log("Upgrade script " + argv[0] + " started");

  // loop through all tasks and execute them
  console.log("Found " + tasks.length + " tasks to run...");
  for (var i in tasks) {
    var task = tasks[i];

    console.log("Executing task #" + i + ": " + task.description);

    var result = task.code();

    if (! result) {
      console.error("Task failed. Aborting upgrade script.");
      console.error("Please fix the problem and try running the upgrade script again.");
      return 1;
    }
    else {
      console.log("Task successful");
    }
  }

  console.log("Upgrade script successfully finished");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
