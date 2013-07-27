/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, regexp: true, plusplus: true, continue: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx User management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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

var arangodb = require("org/arangodb");
var db = require("org/arangodb").db;
var crypto = require("org/arangodb/crypto");
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

function cloneDocument (obj) {
  "use strict";

  if (obj === null || typeof(obj) !== "object") {
    return obj;
  }
 
  var copy, a; 
  if (Array.isArray(obj)) {
    copy = [ ];
    obj.forEach(function (i) {
      copy.push(cloneDocument(i));
    });
  }
  else if (obj instanceof Object) {
    copy = { };
    for (a in obj) {
      if (obj.hasOwnProperty(a)) {
        copy[a] = cloneDocument(obj[a]);
      }
    }
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the plain text password matches the encoded one
////////////////////////////////////////////////////////////////////////////////

function checkPassword (plain, encoded) {
  'use strict';

  var salted = encoded.substr(3, 8) + plain;
  var hex = crypto.sha256(salted);

  return (encoded.substr(12) === hex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief encodes a password
////////////////////////////////////////////////////////////////////////////////

function encodePassword (password) {
  'use strict';

  var salt;
  var encoded;

  var random = crypto.rand();
  if (random === undefined) {
    random = "time:" + internal.time();
  }
  else {
    random = "random:" + random;
  }

  salt = crypto.sha256(random);
  salt = salt.substr(0,8);

  encoded = "$1$" + salt + "$" + crypto.sha256(salt + password);
   
  return encoded;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        FOXX-USERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function FoxxUsers (applicationContext) {
  'use strict';

  this._collectionName = applicationContext.collectionName("users");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the collection exists
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype._requireCollection = function () {
  'use strict';

  var c = db._collection(this._collectionName);

  if (! c) { 
    throw new Error("users collection not found");
  }

  return c;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a user identifier
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype._validateIdentifier = function (identifier, allowObject) {
  'use strict';

  if (allowObject) {
    if (typeof identifier === 'object' && identifier.hasOwnProperty("identifier")) {
      identifier = identifier.identifier;
    }
  }

  if (typeof identifier !== 'string') {
    throw new TypeError("invalid type for 'identifier'");
  }

  if (identifier.length === 0) {
    throw new Error("invalid user identifier");
  }

  return identifier;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the users collection
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.setup = function () {
  'use strict';

  if (! db._collection(this._collectionName)) {
    db._create(this._collectionName);
  }
  
  var c = db._collection(this._collectionName);

  c.ensureUniqueConstraint("identifier");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down the users collection
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.teardown = function () {
  'use strict';

  var c = db._collection(this._collectionName);

  if (c) {
    c.drop();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes all users
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.flush = function () {
  'use strict';
 
  this._requireCollection().truncate();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief add a user
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.add = function (identifier, password, active, data) {
  'use strict';
 
  var c = this._requireCollection();
  identifier = this._validateIdentifier(identifier, false);
  
  if (typeof password !== 'string') {
    throw new TypeError("invalid type for 'password'");
  }

  if (active !== undefined && typeof active !== "boolean") {
    throw new TypeError("invalid type for 'active'");
  }

  var user = {
    identifier: identifier,
    password:   encodePassword(password),
    active:     active || true,
    data:       data || { }
  };

  db._executeTransaction({
    collections: {
      write: c.name()
    },
    action: function (params) {
      var c = require("internal").db._collection(params.cn);
      var u = c.firstExample({ identifier: params.user.identifier });

      if (u === null) {
        c.save(params.user);
      }
      else {
        c.replace(u._key, params.user);
      }
    },
    params: {
      cn: c.name(),
      user: user
    }
  });

  delete user.password;

  return user;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief update a user
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.updateData = function (identifier, data) {
  'use strict';

  var c = this._requireCollection();
  identifier = this._validateIdentifier(identifier, true);

  db._executeTransaction({
    collections: {
      write: c.name()
    },
    action: function (params) {
      var c = require("internal").db._collection(params.cn);
      var u = c.firstExample({ identifier: params.identifier });

      if (u === null) {
        throw new Error("user not found");
      }
      c.update(u._key, params.data, true, false);
    },
    params: {
      cn: c.name(),
      identifier: identifier,
      data: data || { }
    }
  });

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief set activity flag
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.setActive = function (identifier, active) {
  'use strict';
 
  var c = this._requireCollection();
  identifier = this._validateIdentifier(identifier, true);

  var user = c.firstExample({ identifier: identifier });

  if (user === null) {
    require("internal").print("identifier not found");
    return false;
  }

  // must clone because shaped json cannot be modified
  var doc = cloneDocument(user);
  doc.active = active;
  c.update(doc._key, doc, true, false);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief set password
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.setPassword = function (identifier, password) {
  'use strict';
 
  var c = this._requireCollection();
  identifier = this._validateIdentifier(identifier, true);
  
  if (typeof password !== 'string') {
    throw new TypeError("invalid type for 'password'");
  }

  var user = c.firstExample({ identifier: identifier });

  if (user === null) {
    return false;
  }

  var doc = cloneDocument(user);
  doc.password = encodePassword(password);
  c.update(doc._key, doc, true, false);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a user
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.remove = function (identifier) {
  'use strict';
 
  var c = this._requireCollection();
  identifier = this._validateIdentifier(identifier, true);

  var user = c.firstExample({ identifier: identifier });

  if (user === null) {
    return false;
  }

  try {
    c.remove(user._key);
  }
  catch (err) {
  }

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a user
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.get = function (identifier) {
  'use strict';
 
  var c = this._requireCollection();
  identifier = this._validateIdentifier(identifier, true);

  var user = c.firstExample({ identifier: identifier });

  if (user === null) {
    throw new Error("user not found");
  }

  delete user.password;
  
  return user;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a user can log in
////////////////////////////////////////////////////////////////////////////////

FoxxUsers.prototype.checkLogin = function (identifier, password) {
  'use strict';
 
  var c = this._requireCollection();
  identifier = this._validateIdentifier(identifier, false);

  var user = c.firstExample({ identifier: identifier });

  if (user === null) {
    return false;
  }

  if (! user.active) {
    return false;
  }

  if (! checkPassword(password, user.password)) {
    return false;
  }

  delete user.password;
  
  return user;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    module exports
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

exports.FoxxUsers = FoxxUsers;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
