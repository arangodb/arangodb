'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Foxx Authentication
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann, Lucas Dohmen
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var db = require('@arangodb').db,
  crypto = require('@arangodb/crypto'),
  internal = require('internal'),
  is = require('@arangodb/is'),
  _ = require('lodash'),
  errors = internal.errors,
  defaultsFor = {};

function createAuthenticationMiddleware (auth, applicationContext) {
  return function (req) {
    var users = new Users(applicationContext),
      authResult = auth.authenticate(req);

    if (authResult.errorNum === errors.ERROR_NO_ERROR.code) {
      req.currentSession = authResult.session;
      req.user = users.get(authResult.session.identifier);
    } else {
      req.currentSession = null;
      req.user = null;
    }
  };
}

function createSessionUpdateMiddleware () {
  return function (req) {
    var session = req.currentSession;

    if (is.existy(session)) {
      session.update();
    }
  };
}

function createAuthObject (applicationContext, opts) {
  var sessions,
    cookieAuth,
    auth,
    options = opts || {};

  checkAuthenticationOptions(options);

  sessions = new Sessions(applicationContext, options);
  cookieAuth = new CookieAuthentication(applicationContext, options);
  auth = new Authentication(applicationContext, sessions, cookieAuth);

  return auth;
}

function checkAuthenticationOptions (options) {
  if (options.type !== 'cookie') {
    throw new Error('Currently only the following auth types are supported: cookie');
  }
  if (is.falsy(options.cookieLifetime)) {
    throw new Error('Please provide the cookieLifetime');
  }
  if (is.falsy(options.sessionLifetime)) {
    throw new Error('Please provide the sessionLifetime');
  }
}

defaultsFor.login = {
  usernameField: 'username',
  passwordField: 'password',

  onSuccess: function (req, res) {
    res.json({
      user: req.user.identifier,
      key: req.currentSession._key
    });
  },

  onError: function (req, res) {
    res.status(401);
    res.json({
      error: 'Username or Password was wrong'
    });
  }
};

function createStandardLoginHandler (auth, users, opts) {
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
}

defaultsFor.logout = {
  onSuccess: function (req, res) {
    res.json({
      notice: 'Logged out!'
    });
  },

  onError: function (req, res) {
    res.status(401);
    res.json({
      error: 'No session was found'
    });
  }
};

function createStandardLogoutHandler (auth, opts) {
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
}

defaultsFor.registration = {
  usernameField: 'username',
  passwordField: 'password',
  acceptedAttributes: [],
  defaultAttributes: {},

  onSuccess: function (req, res) {
    res.json({
      user: req.user
    });
  },

  onError: function (req, res) {
    res.status(401);
    res.json({
      error: 'Registration failed'
    });
  }
};

function createStandardRegistrationHandler (auth, users, opts) {
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
}

defaultsFor.changePassword = {
  passwordField: 'password',

  onSuccess: function (req, res) {
    res.json({
      notice: 'Changed password!'
    });
  },

  onError: function (req, res) {
    res.status(401);
    res.json({
      error: 'No session was found'
    });
  }
};

function createStandardChangePasswordHandler (users, opts) {
  var options = _.defaults(opts || {}, defaultsFor.changePassword);

  return function (req, res) {
    var password = req.body()[options.passwordField],
      successfull = false;

    if (is.existy(req.currentSession)) {
      successfull = users.setPassword(req.currentSession.identifier, password);
      if (successfull) {
        options.onSuccess(req, res);
      } else {
        options.onError(req, res);
      }
    } else {
      options.onError(req, res);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

function generateToken () {
  return internal.genRandomAlphaNumbers(32);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief deep-copies a document
// //////////////////////////////////////////////////////////////////////////////

function cloneDocument (obj) {
  var copy, a;

  if (obj === null || typeof obj !== 'object') {
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
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks whether the plain text password matches the encoded one
// //////////////////////////////////////////////////////////////////////////////

function checkPassword (plain, encoded) {
  var salted = encoded.substr(3, 8) + plain,
    hex = crypto.sha256(salted);

  return (encoded.substr(12) === hex);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief encodes a password
// //////////////////////////////////////////////////////////////////////////////

function encodePassword (password) {
  var salt,
    encoded,
    random;

  random = crypto.rand();
  if (random === undefined) {
    random = 'time:' + internal.time();
  } else {
    random = 'random:' + random;
  }

  salt = crypto.sha256(random);
  salt = salt.substr(0, 8);

  encoded = '$1$' + salt + '$' + crypto.sha256(salt + password);

  return encoded;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

function Users (applicationContext, options) {
  this._options = options || {};

  if (this._options.hasOwnProperty('collectionName')) {
    this._collectionName = this._options.collectionName;
  } else {
    this._collectionName = applicationContext.collectionName('users');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the collection
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.storage = function () {
  var collection = db._collection(this._collectionName);

  if (!collection) {
    throw new Error('users collection not found');
  }

  return collection;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief validate a user identifier
// //////////////////////////////////////////////////////////////////////////////

Users.prototype._validateIdentifier = function (identifier, allowObject) {
  if (allowObject) {
    if (typeof identifier === 'object' && identifier.hasOwnProperty('identifier')) {
      identifier = identifier.identifier;
    }
  }

  if (typeof identifier !== 'string') {
    throw new TypeError("invalid type for 'identifier'");
  }

  if (identifier.length === 0) {
    throw new Error('invalid user identifier');
  }

  return identifier;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets up the users collection
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.setup = function (options) {
  if (!db._collection(this._collectionName)) {
    db._create(this._collectionName);
  }

  this.storage().ensureIndex({ type: 'hash', fields: [ 'identifier' ], sparse: true });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief tears down the users collection
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.teardown = function () {
  var c = db._collection(this._collectionName);

  if (c) {
    c.drop();
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief flushes all users
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.flush = function () {
  this.storage().truncate();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief add a user
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.add = function (identifier, password, active, data) {
  var c = this.storage(),
    user;

  identifier = this._validateIdentifier(identifier, false);

  if (typeof password !== 'string') {
    throw new TypeError("invalid type for 'password'");
  }

  if (active !== undefined && typeof active !== 'boolean') {
    throw new TypeError("invalid type for 'active'");
  }
  if (active === undefined) {
    active = true;
  }

  user = {
    identifier: identifier,
    password: encodePassword(password),
    active: active,
    data: data || {}
  };

  db._executeTransaction({
    collections: {
      exclusive: c.name()
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief update a user
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.updateData = function (identifier, data) {
  var c = this.storage();

  identifier = this._validateIdentifier(identifier, true);

  db._executeTransaction({
    collections: {
      exclusive: c.name()
    },
    action: function (params) {
      var c = db._collection(params.cn),
        u = c.firstExample({ identifier: params.identifier });

      if (u === null) {
        throw new Error('user not found');
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief set activity flag
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.setActive = function (identifier, active) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief set password
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.setPassword = function (identifier, password) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief remove a user
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.remove = function (identifier) {
  var c = this.storage(),
    user;

  identifier = this._validateIdentifier(identifier, true);

  user = c.firstExample({ identifier: identifier });

  if (user === null) {
    return false;
  }

  try {
    c.remove(user._key);
  } catch (err) {}

  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns a user
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.get = function (identifier) {
  var c = this.storage(),
    user;

  identifier = this._validateIdentifier(identifier, true);

  user = c.firstExample({ identifier: identifier });

  if (user === null) {
    throw new Error('user not found');
  }

  delete user.password;

  return user;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks if a user exists
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.exists = function (identifier) {
  var c = this.storage(),
    user;

  identifier = this._validateIdentifier(identifier, true);
  user = c.firstExample({ identifier: identifier });
  return (user !== null);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief check whether a user is valid
// //////////////////////////////////////////////////////////////////////////////

Users.prototype.isValid = function (identifier, password) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

function Sessions (applicationContext, options) {
  this._applicationContext = applicationContext;
  this._options = options || {};

  if (!this._options.hasOwnProperty('minUpdateResoultion')) {
    this._options.minUpdateResolution = 10;
  }

  if (this._options.hasOwnProperty('collectionName')) {
    this._collectionName = this._options.collectionName;
  } else {
    this._collectionName = applicationContext.collectionName('sessions');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief create a session object from a document
// //////////////////////////////////////////////////////////////////////////////

Sessions.prototype._toObject = function (session) {
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
        newExpires = internal.time() + that._options.sessionLifetime;

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets up the sessions
// //////////////////////////////////////////////////////////////////////////////

Sessions.prototype.setup = function (options) {
  if (!db._collection(this._collectionName)) {
    db._create(this._collectionName);
  }

  this.storage().ensureIndex({ type: 'hash', fields: ['identifier']});
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief tears down the sessions
// //////////////////////////////////////////////////////////////////////////////

Sessions.prototype.teardown = function () {
  var c = db._collection(this._collectionName);

  if (c) {
    c.drop();
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the collection
// //////////////////////////////////////////////////////////////////////////////

Sessions.prototype.storage = function () {
  var collection = db._collection(this._collectionName);

  if (!collection) {
    throw new Error('sessions collection not found');
  }

  return collection;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief generate a new session
// //////////////////////////////////////////////////////////////////////////////

Sessions.prototype.generate = function (identifier, data) {
  var storage, token, session;

  if (typeof identifier !== 'string' || identifier.length === 0) {
    throw new TypeError("invalid type for 'identifier'");
  }

  if (!this._options.hasOwnProperty('sessionLifetime')) {
    throw new Error("no value specified for 'sessionLifetime'");
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
      expires: internal.time() + this._options.sessionLifetime,
      identifier: identifier,
      data: data || {}
    };

    try {
      storage.save(session);
      return this._toObject(session);
    } catch (err) {
      // we might have generated the same key again
      if (err.hasOwnProperty('errorNum') &&
        err.errorNum === internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code) {
        // duplicate key, try again
        continue;
      }

      throw err;
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief update a session
// //////////////////////////////////////////////////////////////////////////////

Sessions.prototype.update = function (token, data) {
  this.storage().update(token, {
    expires: internal.time() + this._options.sessionLifetime,
    data: data
  }, true, false);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief terminate a session
// //////////////////////////////////////////////////////////////////////////////

Sessions.prototype.terminate = function (token) {
  try {
    this.storage().remove(token);
  } catch (err) {
    // some error, e.g. document not found. we don't care
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief get an existing session
// //////////////////////////////////////////////////////////////////////////////

Sessions.prototype.get = function (token) {
  var storage = this.storage(),
    session;

  try {
    session = storage.document(token);

    if (session.expires >= internal.time()) {
      // session still valid

      return {
        errorNum: internal.errors.ERROR_NO_ERROR.code,
        session: this._toObject(session)
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

function CookieAuthentication (applicationContext, options) {
  options = options || {};

  this._applicationContext = applicationContext;

  this._options = {
    name: options.name || this._applicationContext.name + '-session',
    cookieLifetime: options.cookieLifetime || 3600,
    path: options.path || '/',
    domain: options.domain || undefined,
    secure: options.secure || false,
    httpOnly: options.httpOnly || false
  };

  this._collectionName = applicationContext.collectionName('sessions');
  this._collection = null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get a cookie from the request
// //////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.getTokenFromRequest = function (req) {
  if (!req.hasOwnProperty('cookies')) {
    return null;
  }

  if (!req.cookies.hasOwnProperty(this._options.name)) {
    return null;
  }

  return req.cookies[this._options.name];
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief register a cookie in the request
// //////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.setCookie = function (res, value) {
  var name = this._options.name,
    cookie,
    i,
    n;

  cookie = {
    name: name,
    value: value,
    lifeTime: (value === null || value === '') ? 0 : this._options.cookieLifetime,
    path: this._options.path,
    secure: this._options.secure,
    domain: this._options.domain,
    httpOnly: this._options.httpOnly
  };

  if (!res.hasOwnProperty('cookies')) {
    res.cookies = [];
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief check whether the request contains authentication data
// //////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.getAuthenticationData = function (req) {
  var token = this.getTokenFromRequest(req);

  if (token === null) {
    return null;
  }

  return {
    token: token
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief generate authentication data
// //////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.beginSession = function (req, res, token) {
  this.setCookie(res, token);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief delete authentication data
// //////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.endSession = function (req, res) {
  this.setCookie(res, '');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief update authentication data
// //////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.updateSession = function (req, res, session) {
  // update the cookie (expire date)
  this.setCookie(res, session._key);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief check whether the authentication handler is responsible
// //////////////////////////////////////////////////////////////////////////////

CookieAuthentication.prototype.isResponsible = function () {
  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

function Authentication (applicationContext, sessions, authenticators) {
  this._applicationContext = applicationContext;
  this._sessions = sessions;

  if (!Array.isArray(authenticators)) {
    authenticators = [ authenticators ];
  }

  this._authenticators = authenticators;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs the authentication
// //////////////////////////////////////////////////////////////////////////////

Authentication.prototype.authenticate = function (req) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief begin a session
// //////////////////////////////////////////////////////////////////////////////

Authentication.prototype.beginSession = function (req, res, identifier, data) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief terminate a session
// //////////////////////////////////////////////////////////////////////////////

Authentication.prototype.endSession = function (req, res, token) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief update a session
// //////////////////////////////////////////////////////////////////////////////

Authentication.prototype.updateSession = function (req, res, session) {
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

// http://stackoverflow.com/questions/783818/how-do-i-create-a-custom-error-in-javascript

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

function UserAlreadyExistsError (message) {
  this.message = message || 'User already exists';
  this.statusCode = 400;
}

UserAlreadyExistsError.prototype = new Error();

function UnauthorizedError (message) {
  this.message = message || 'Unauthorized';
  this.statusCode = 401;
}

UnauthorizedError.prototype = new Error();

exports.Users = Users;
exports.Sessions = Sessions;
exports.CookieAuthentication = CookieAuthentication;
exports.Authentication = Authentication;
exports.UnauthorizedError = UnauthorizedError;
exports.UserAlreadyExistsError = UserAlreadyExistsError;
exports.createStandardLoginHandler = createStandardLoginHandler;
exports.createStandardLogoutHandler = createStandardLogoutHandler;
exports.createStandardRegistrationHandler = createStandardRegistrationHandler;
exports.createStandardChangePasswordHandler = createStandardChangePasswordHandler;
exports.createAuthenticationMiddleware = createAuthenticationMiddleware;
exports.createSessionUpdateMiddleware = createSessionUpdateMiddleware;
exports.createAuthObject = createAuthObject;
