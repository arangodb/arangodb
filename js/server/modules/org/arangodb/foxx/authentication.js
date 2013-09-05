/*jslint indent: 2, nomen: true, maxlen: 120, plusplus: true, continue: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Authentication
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
/// @author Jan Steemann, Lucas Dohmen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb"),
  db = require("org/arangodb").db,
  crypto = require("org/arangodb/crypto"),
  internal = require("internal"),
  is = require("org/arangodb/is"),
  _ = require("underscore"),
  errors = require("internal").errors,
  defaultsFor = {},
  checkAuthenticationOptions,
  createStandardLoginHandler,
  createStandardLogoutHandler,
  createStandardRegistrationHandler,
  createAuthenticationMiddleware,
  createSessionUpdateMiddleware,
  createAuthObject,
  generateToken,
  cloneDocument,
  checkPassword,
  encodePassword,
  Users,
  Sessions,
  CookieAuthentication,
  Authentication,
  UserAlreadyExistsError,
  UnauthorizedError;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

createAuthenticationMiddleware = function (auth, applicationContext) {
  'use strict';
  return function (req, res) {
    var users = new Users(applicationContext),
      authResult = auth.authenticate(req);

    if (authResult.errorNum === errors.ERROR_NO_ERROR) {
      req.currentSession = authResult.session;
      req.user = users.get(authResult.session.identifier);
    } else {
      req.currentSession = null;
      req.user = null;
    }
  };
};

createSessionUpdateMiddleware = function () {
  'use strict';
  return function (req, res) {
    var session = req.currentSession;

    if (is.existy(session)) {
      session.update();
    }
  };
};

createAuthObject = function (applicationContext, opts) {
  'use strict';
  var sessions,
    cookieAuth,
    auth,
    options = opts || {};

  checkAuthenticationOptions(options);

  sessions = new Sessions(applicationContext, {
    lifetime: options.sessionLifetime
  });

  cookieAuth = new CookieAuthentication(applicationContext, {
    lifetime: options.cookieLifetime,
    name: options.cookieName
  });

  auth = new Authentication(applicationContext, sessions, cookieAuth);

  return auth;
};

checkAuthenticationOptions = function (options) {
  'use strict';
  if (options.type !== "cookie") {
    throw new Error("Currently only the following auth types are supported: cookie");
  }
  if (is.falsy(options.cookieLifetime)) {
    throw new Error("Please provide the cookieLifetime");
  }
  if (is.falsy(options.cookieName)) {
    throw new Error("Please provide the cookieName");
  }
  if (is.falsy(options.sessionLifetime)) {
    throw new Error("Please provide the sessionLifetime");
  }
};

defaultsFor.login = {
  usernameField: "username",
  passwordField: "password",

  onSuccess: function (req, res) {
    'use strict';
    res.json({
      user: req.user.identifier,
      key: req.currentSession._key
    });
  },

  onError: function (req, res) {
    'use strict';
    res.status(401);
    res.json({
      error: "Username or Password was wrong"
    });
  }
};

createStandardLoginHandler = function (auth, users, opts) {
  'use strict';
  var options = _.defaults(opts || {}, defaultsFor.login);

  return function (req, res) {
    var username = req.body()[options.usernameField],
      password = req.body()[options.passwordField];

    if (users.isValid(username, password)) {
      req.currentSession = auth.beginSession(req, res, username, {});
      req.user = users.get(req.currentSession.identifier);
      options.onSuccess(req, res);
    } else {
      options.onError(req, res);
    }
  };
};

defaultsFor.logout = {
  onSuccess: function (req, res) {
    'use strict';
    res.json({
      notice: "Logged out!"
    });
  },

  onError: function (req, res) {
    'use strict';
    res.status(401);
    res.json({
      error: "No session was found"
    });
  }
};

createStandardLogoutHandler = function (auth, opts) {
  'use strict';
  var options = _.defaults(opts || {}, defaultsFor.logout);

  return function (req, res) {
    if (is.existy(req.currentSession)) {
      auth.endSession(req, res, req.currentSession._key);
      req.user = null;
      req.currentSession = null;
      options.onSuccess(req, res);
    } else {
      options.onError(req, res);
    }
  };
};

defaultsFor.registration = {
  usernameField: "username",
  passwordField: "password",
  acceptedAttributes: [],
  defaultAttributes: {},

  onSuccess: function (req, res) {
    'use strict';
    res.json({
      user: req.user
    });
  },

  onError: function (req, res) {
    'use strict';
    res.status(401);
    res.json({
      error: "Registration failed"
    });
  }
};

createStandardRegistrationHandler = function (auth, users, opts) {
  'use strict';
  var options = _.defaults(opts || {}, defaultsFor.registration);

  return function (req, res) {
    var username = req.body()[options.usernameField],
      password = req.body()[options.passwordField],
      data = _.defaults({}, options.defaultAttributes);

    options.acceptedAttributes.forEach(function (attributeName) {
      var val = req.body()[attributeName];
      if (is.existy(val)) {
        data[attributeName] = val;
      }
    });

    if (users.exists(username)) {
      throw new UserAlreadyExistsError();
    }

    req.user = users.add(username, password, true, data);
    req.currentSession = auth.beginSession(req, res, username, {});
    options.onSuccess(req, res);
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

generateToken = function () {
  'use strict';

  return internal.genRandomAlphaNumbers(32);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief deep-copies a document
////////////////////////////////////////////////////////////////////////////////

cloneDocument = function (obj) {
  "use strict";
  var copy, a;

  if (obj === null || typeof obj !== "object") {
    return obj;
  }

  if (Array.isArray(obj)) {
    copy = [];
    obj.forEach(function (i) {
      copy.push(cloneDocument(i));
    });
  } else if (obj instanceof Object) {
    copy = {};
    for (a in obj) {
      if (obj.hasOwnProperty(a)) {
        copy[a] = cloneDocument(obj[a]);
      }
    }
  }

  return copy;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the plain text password matches the encoded one
////////////////////////////////////////////////////////////////////////////////

checkPassword = function (plain, encoded) {
  'use strict';
  var salted = encoded.substr(3, 8) + plain,
    hex = crypto.sha256(salted);

  return (encoded.substr(12) === hex);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief encodes a password
////////////////////////////////////////////////////////////////////////////////

encodePassword = function (password) {
  'use strict';
  var salt,
    encoded,
    random;

  random = crypto.rand();
  if (random === undefined) {
    random = "time:" + internal.time();
  } else {
    random = "random:" + random;
  }

  salt = crypto.sha256(random);
  salt = salt.substr(0, 8);

  encoded = "$1$" + salt + "$" + crypto.sha256(salt + password);

  return encoded;
};

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

Users = function (applicationContext, options) {
  'use strict';

  this._options = options || {};
  this._collection = null;

  if (this._options.hasOwnProperty("collectionName")) {
    this._collectionName = this._options.collectionName;
  } else {
    this._collectionName = applicationContext.collectionName("users");
  }
};

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
/// @brief returns the collection
////////////////////////////////////////////////////////////////////////////////

Users.prototype.storage = function () {
  'use strict';

  if (this._collection === null) {
    this._collection = db._collection(this._collectionName);

    if (!this._collection) {
      throw new Error("users collection not found");
    }
  }

  return this._collection;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a user identifier
////////////////////////////////////////////////////////////////////////////////

Users.prototype._validateIdentifier = function (identifier, allowObject) {
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

Users.prototype.setup = function (options) {
  'use strict';
  var journalSize,
    createOptions;

  if (typeof options === 'object' && options.hasOwnProperty('journalSize')) {
    journalSize = options.journalSize;
  }

  createOptions = {
    journalSize : journalSize || 2 * 1024 * 1024
  };

  if (!db._collection(this._collectionName)) {
    db._create(this._collectionName, createOptions);
  }

  this.storage().ensureUniqueConstraint("identifier");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down the users collection
////////////////////////////////////////////////////////////////////////////////

Users.prototype.teardown = function () {
  'use strict';
  var c = db._collection(this._collectionName);

  if (c) {
    c.drop();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes all users
////////////////////////////////////////////////////////////////////////////////

Users.prototype.flush = function () {
  'use strict';

  this.storage().truncate();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief add a user
////////////////////////////////////////////////////////////////////////////////

Users.prototype.add = function (identifier, password, active, data) {
  'use strict';
  var c = this.storage(),
    user;

  identifier = this._validateIdentifier(identifier, false);

  if (typeof password !== 'string') {
    throw new TypeError("invalid type for 'password'");
  }

  if (active !== undefined && typeof active !== "boolean") {
    throw new TypeError("invalid type for 'active'");
  }
  if (active === undefined) {
    active = true;
  }

  user = {
    identifier: identifier,
    password:   encodePassword(password),
    active:     active,
    data:       data || {}
  };

  db._executeTransaction({
    collections: {
      write: c.name()
    },
    action: function (params) {
      var c = db._collection(params.cn),
        u = c.firstExample({ identifier: params.user.identifier });

      if (u === null) {
        c.save(params.user);
      } else {
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

Users.prototype.updateData = function (identifier, data) {
  'use strict';
  var c = this.storage();

  identifier = this._validateIdentifier(identifier, true);

  db._executeTransaction({
    collections: {
      write: c.name()
    },
    action: function (params) {
      var c = db._collection(params.cn),
        u = c.firstExample({ identifier: params.identifier });

      if (u === null) {
        throw new Error("user not found");
      }
      c.update(u._key, params.data, true, false);
    },
    params: {
      cn: c.name(),
      identifier: identifier,
      data: data || {}
    }
  });

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief set activity flag
////////////////////////////////////////////////////////////////////////////////

Users.prototype.setActive = function (identifier, active) {
  'use strict';
  var c = this.storage(),
    user,
    doc;

  identifier = this._validateIdentifier(identifier, true);

  user = c.firstExample({ identifier: identifier });

  if (user === null) {
    return false;
  }

  // must clone because shaped json cannot be modified
  doc = cloneDocument(user);
  doc.active = active;
  c.update(doc._key, doc, true, false);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief set password
////////////////////////////////////////////////////////////////////////////////

Users.prototype.setPassword = function (identifier, password) {
  'use strict';
  var c = this.storage(),
    user,
    doc;

  identifier = this._validateIdentifier(identifier, true);

  if (typeof password !== 'string') {
    throw new TypeError("invalid type for 'password'");
  }

  user = c.firstExample({ identifier: identifier });

  if (user === null) {
    return false;
  }

  doc = cloneDocument(user);
  doc.password = encodePassword(password);
  c.update(doc._key, doc, true, false);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a user
////////////////////////////////////////////////////////////////////////////////

Users.prototype.remove = function (identifier) {
  'use strict';
  var c = this.storage(),
    user;

  identifier = this._validateIdentifier(identifier, true);

  user = c.firstExample({ identifier: identifier });

  if (user === null) {
    return false;
  }

  try {
    c.remove(user._key);
  } catch (err) {
  }

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a user
////////////////////////////////////////////////////////////////////////////////

Users.prototype.get = function (identifier) {
  'use strict';
  var c = this.storage(),
    user;

  identifier = this._validateIdentifier(identifier, true);

  user = c.firstExample({ identifier: identifier });

  if (user === null) {
    throw new Error("user not found");
  }

  delete user.password;

  return user;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a user exists
////////////////////////////////////////////////////////////////////////////////

Users.prototype.exists = function (identifier) {
  'use strict';
  var c = this.storage(),
    user;

  identifier = this._validateIdentifier(identifier, true);
  user = c.firstExample({ identifier: identifier });
  return (user !== null);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a user is valid 
////////////////////////////////////////////////////////////////////////////////

Users.prototype.isValid = function (identifier, password) {
  'use strict';
  var c = this.storage(),
    user;

  identifier = this._validateIdentifier(identifier, false);

  user = c.firstExample({ identifier: identifier });

  if (user === null) {
    return false;
  }

  if (!user.active) {
    return false;
  }

  if (!checkPassword(password, user.password)) {
    return false;
  }

  delete user.password;

  return user;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                          SESSIONS
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

Sessions = function (applicationContext, options) {
  'use strict';

  this._applicationContext = applicationContext;
  this._options = options || {};
  this._collection = null;

  if (!this._options.hasOwnProperty("minUpdateResoultion")) {
    this._options.minUpdateResolution = 10;
  }

  if (this._options.hasOwnProperty("collectionName")) {
    this._collectionName = this._options.collectionName;
  } else {
    this._collectionName = applicationContext.collectionName("sessions");
  }
};

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
/// @brief create a session object from a document
////////////////////////////////////////////////////////////////////////////////

Sessions.prototype._toObject = function (session) {
  'use strict';
  var that = this;

  return {
    _changed: false,
    _key: session._key,
    identifier: session.identifier,
    expires: session.expires,
    data: session.data,

    set: function (key, value) {
      this.data[key] = value;
      this._changed = true;
    },

    get: function (key) {
      return this.data[key];
    },

    update: function () {
      var oldExpires, newExpires;

      if (!this._changed) {
        oldExpires = this.expires;
        newExpires = internal.time() + that._options.lifetime;

        if (newExpires - oldExpires > that._options.minUpdateResolution) {
          this.expires = newExpires;
          this._changed = true;
        }
      }

      if (this._changed) {
        that.update(this._key, this.data);
        this._changed = false;
      }
    }
  };
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
/// @brief sets up the sessions
////////////////////////////////////////////////////////////////////////////////

Sessions.prototype.setup = function (options) {
  'use strict';
  var journalSize,
    createOptions;

  if (typeof options === 'object' && options.hasOwnProperty('journalSize')) {
    journalSize = options.journalSize;
  }

  createOptions = {
    journalSize : journalSize || 4 * 1024 * 1024
  };

  if (!db._collection(this._collectionName)) {
    db._create(this._collectionName, createOptions);
  }

  this.storage().ensureHashIndex("identifier");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down the sessions
////////////////////////////////////////////////////////////////////////////////

Sessions.prototype.teardown = function () {
  'use strict';
  var c = db._collection(this._collectionName);

  if (c) {
    c.drop();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

Sessions.prototype.storage = function () {
  'use strict';

  if (this._collection === null) {
    this._collection = db._collection(this._collectionName);

    if (!this._collection) {
      throw new Error("sessions collection not found");
    }
  }

  return this._collection;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a new session
////////////////////////////////////////////////////////////////////////////////

Sessions.prototype.generate = function (identifier, data) {
  'use strict';
  var storage, token, session;

  if (typeof identifier !== "string" || identifier.length === 0) {
    throw new TypeError("invalid type for 'identifier'");
  }

  if (!this._options.hasOwnProperty("lifetime")) {
    throw new Error("no value specified for 'lifetime'");
  }

  storage = this.storage();

  if (!this._options.allowMultiple) {
    // remove previous existing sessions
    storage.byExample({ identifier: identifier }).toArray().forEach(function (s) {
      storage.remove(s);
    });
  }

  while (true) {
    token = generateToken();
    session = {
      _key: token,
      expires: internal.time() + this._options.lifetime,
      identifier: identifier,
      data: data || {}
    };

    try {
      storage.save(session);
      return this._toObject(session);
    } catch (err) {
      // we might have generated the same key again
      if (err.hasOwnProperty("errorNum") &&
          err.errorNum === internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
        // duplicate key, try again
        continue;
      }

      throw err;
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief update a session
////////////////////////////////////////////////////////////////////////////////

Sessions.prototype.update = function (token, data) {
  'use strict';

  this.storage().update(token, {
    expires: internal.time() + this._options.lifetime,
    data: data
  }, true, false);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief terminate a session
////////////////////////////////////////////////////////////////////////////////

Sessions.prototype.terminate = function (token) {
  'use strict';

  try {
    this.storage().remove(token);
  } catch (err) {
    // some error, e.g. document not found. we don't care
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get an existing session
////////////////////////////////////////////////////////////////////////////////

Sessions.prototype.get = function (token) {
  'use strict';
  var storage = this.storage(),
    session,
    lifetime;

  try {
    session = storage.document(token);

    if (session.expires >= internal.time()) {
      // session still valid

      lifetime = this._options.lifetime;

      return {
        errorNum: internal.errors.ERROR_NO_ERROR,
        session : this._toObject(session)
      };
    }

    // expired
    return {
      errorNum: internal.errors.ERROR_SESSION_EXPIRED.code
    };
  } catch (err) {
    // document not found etc.
  }

  return {
    errorNum: internal.errors.ERROR_SESSION_UNKNOWN.code
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             COOKIE AUTHENTICATION
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

CookieAuthentication = function (applicationContext, options) {
  'use strict';

  options = options || {};

  this._applicationContext = applicationContext;

  this._options = {
    name: options.name || this._applicationContext.name + "-session",
    lifetime: options.lifetime || 3600,
    path: options.path || this._applicationContext.mount,
    domain: options.path || undefined,
    secure: options.secure || false,
    httpOnly: options.httpOnly || false
  };

  this._collectionName = applicationContext.collectionName("sessions");
  this._collection = null;
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
/// @brief get a cookie from the request
////////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.getTokenFromRequest = function (req) {
  'use strict';

  if (!req.hasOwnProperty("cookies")) {
    return null;
  }

  if (!req.cookies.hasOwnProperty(this._options.name)) {
    return null;
  }

  return req.cookies[this._options.name];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief register a cookie in the request
////////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.setCookie = function (res, value) {
  'use strict';
  var name = this._options.name,
    cookie,
    i,
    n;

  cookie = {
    name: name,
    value: value,
    lifeTime: (value === null || value === "") ? 0 : this._options.lifetime,
    path: this._options.path,
    secure: this._options.secure,
    domain: this._options.domain,
    httpOnly: this._options.httpOnly
  };

  if (!res.hasOwnProperty("cookies")) {
    res.cookies = [ ];
  }

  if (!Array.isArray(res.cookies)) {
    res.cookies = [ res.cookies ];
  }

  n = res.cookies.length;
  for (i = 0; i < n; ++i) {
    if (res.cookies[i].name === name) {
      // found existing cookie. overwrite it
      res.cookies[i] = cookie;
      return;
    }
  }

  res.cookies.push(cookie);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the request contains authentication data
////////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.getAuthenticationData = function (req) {
  'use strict';

  var token = this.getTokenFromRequest(req);

  if (token === null) {
    return null;
  }

  return {
    token: token
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief generate authentication data
////////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.beginSession = function (req, res, token, identifier, data) {
  'use strict';

  this.setCookie(res, token);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief delete authentication data
////////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.endSession = function (req, res) {
  'use strict';

  this.setCookie(res, "");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief update authentication data
////////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.updateSession = function (req, res, session) {
  'use strict';

  // update the cookie (expire date)
  this.setCookie(res, session._key);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the authentication handler is responsible
////////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.isResponsible = function (req) {
  'use strict';

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               FOXX AUTHENTICATION
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

Authentication = function (applicationContext, sessions, authenticators) {
  'use strict';

  this._applicationContext = applicationContext;
  this._sessions = sessions;

  if (!Array.isArray(authenticators)) {
    authenticators = [ authenticators ];
  }

  this._authenticators = authenticators;
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
/// @brief runs the authentication
////////////////////////////////////////////////////////////////////////////////

Authentication.prototype.authenticate = function (req) {
  'use strict';
  var i,
    n = this._authenticators.length,
    authenticator,
    data,
    session;

  for (i = 0; i < n; ++i) {
    authenticator = this._authenticators[i];

    data = authenticator.getAuthenticationData(req);

    if (data !== null) {
      session = this._sessions.get(data.token);

      if (session) {
        return session;
      }
    }
  }

  return {
    errorNum: internal.errors.ERROR_SESSION_UNKNOWN.code
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a session
////////////////////////////////////////////////////////////////////////////////

Authentication.prototype.beginSession = function (req, res, identifier, data) {
  'use strict';
  var session = this._sessions.generate(identifier, data),
    i,
    n = this._authenticators.length,
    authenticator;

  for (i = 0; i < n; ++i) {
    authenticator = this._authenticators[i];

    if (authenticator.isResponsible(req) &&
        authenticator.beginSession) {
      authenticator.beginSession(req, res, session._key, identifier, data);
    }
  }

  return session;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief terminate a session
////////////////////////////////////////////////////////////////////////////////

Authentication.prototype.endSession = function (req, res, token) {
  'use strict';
  var i,
    n = this._authenticators.length,
    authenticator;

  for (i = 0; i < n; ++i) {
    authenticator = this._authenticators[i];

    if (authenticator.isResponsible(req) &&
        authenticator.endSession) {
      authenticator.endSession(req, res);
    }
  }

  this._sessions.terminate(token);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief update a session
////////////////////////////////////////////////////////////////////////////////

Authentication.prototype.updateSession = function (req, res, session) {
  'use strict';
  var i,
    n = this._authenticators.length,
    authenticator;

  for (i = 0; i < n; ++i) {
    authenticator = this._authenticators[i];

    if (authenticator.isResponsible(req) &&
        authenticator.updateSession) {
      authenticator.updateSession(req, res, session);
    }
  }

  session.update();
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     custom errors
// -----------------------------------------------------------------------------

// http://stackoverflow.com/questions/783818/how-do-i-create-a-custom-error-in-javascript

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

UserAlreadyExistsError = function (message) {
  'use strict';
  this.message = message || "User already exists";
  this.statusCode = 400;
};

UserAlreadyExistsError.prototype = new Error();

UnauthorizedError = function (message) {
  'use strict';
  this.message = message || "Unauthorized";
  this.statusCode = 401;
};

UnauthorizedError.prototype = new Error();

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

exports.Users                             = Users;
exports.Sessions                          = Sessions;
exports.CookieAuthentication              = CookieAuthentication;
exports.Authentication                    = Authentication;
exports.UnauthorizedError                 = UnauthorizedError;
exports.UserAlreadyExistsError            = UserAlreadyExistsError;
exports.createStandardLoginHandler        = createStandardLoginHandler;
exports.createStandardLogoutHandler       = createStandardLogoutHandler;
exports.createStandardRegistrationHandler = createStandardRegistrationHandler;
exports.createAuthenticationMiddleware    = createAuthenticationMiddleware;
exports.createSessionUpdateMiddleware     = createSessionUpdateMiddleware;
exports.createAuthObject                  = createAuthObject;

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
