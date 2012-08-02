////////////////////////////////////////////////////////////////////////////////
/// @brief arango change password
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates or changes the passwords
////////////////////////////////////////////////////////////////////////////////

function main (argv) {
  var argc = argv.length;
  var internal = require("internal");
  var console = require("console");
  var db = internal.db;
  var users = null;
  var username = null;
  var pasword = null;

  users = db._collection("_users");

  if (db._collection("_users") == null) {
    console.log("creating users collection '_users'");
    users = db._create("_users", { isSystem: true, waitForSync: true });
  }

  internal.output("\n");
  
  if (argc < 1 || argc > 3) {
    internal.output("usage: ", argv[0], " <username> [<password>]\n");
    return 1;
  }

  if (argc == 1) {
    internal.output("Enter User: ");
    username = console.getline();
    
    internal.output("Enter Password: ");
    password = console.getline();
  }
  else if (argc == 2) {
    username = argv[1];
    internal.output("Enter Password: ");
    password = console.getline();
  }
  else if (argc == 3) {
    username = argv[1];
    password = argv[2];
  }
  
  var hash = internal.sha256(password);

  var user = users.firstExample({ user: username });

  if (user == null) {
    users.save({ user: username, password: hash, active: true });
    console.info("created user '%s'", username);
  }
  else {
    var data = user.shallowCopy;
    data.password = hash;

    users.replace(user, data);
    console.info("updated user '%s'", username);
  }

  console.info("password hash '%s'", hash);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
