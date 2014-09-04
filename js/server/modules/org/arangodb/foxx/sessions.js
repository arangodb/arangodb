/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Sessions
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

var Foxx = require('org/arangodb/foxx'),
  crypto = require('org/arangodb/crypto');

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief decorates the controller with session logic
////////////////////////////////////////////////////////////////////////////////

function decorateController(auth, controller) {
  'use strict';
  var cfg = auth.configuration;

  controller.before('/*', function (req) {
    var sessions = auth.getSessionStorage();
    var sid;
    if (cfg.type === 'cookie') {
      sid = req.cookie(cfg.cookieName, cfg.cookieSecret ? {
        signed: {
          secret: cfg.cookieSecret,
          algorithm: cfg.cookieAlgorithm
        }
      } : undefined);
    } else if (cfg.type === 'header') {
      sid = req.headers[cfg.headerName.toLowerCase()];
    }
    if (sid) {
      if (cfg.jwt) {
        sid = crypto.jwtDecode(cfg.jwt.secret, sid, !cfg.jwt.verify);
      }
      try {
        req.session = sessions.get(sid);
      } catch (e) {
        if (!(e instanceof sessions.errors.SessionNotFound)) {
          throw e;
        }
      }
    }
    if (!req.session && cfg.autoCreateSession) {
      req.session = sessions.create();
    }
  });

  controller.after('/*', function (req, res) {
    if (req.session) {
      var sid = req.session.get('_key');
      if (cfg.jwt) {
        sid = crypto.jwtEncode(cfg.jwt.secret, sid, cfg.jwt.algorithm);
      }
      if (cfg.type === 'cookie') {
        res.cookie(cfg.cookieName, sid, {
          ttl: req.session.getTTL() / 1000,
          signed: cfg.cookieSecret ? {
            secret: cfg.cookieSecret,
            algorithm: cfg.cookieAlgorithm
          } : undefined
        });
      } else if (cfg.type === 'header') {
        res.set(cfg.headerName, sid);
      }
    }
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convenience wrapper for session destruction logic
////////////////////////////////////////////////////////////////////////////////

function createDestroySessionHandler(auth, opts) {
  'use strict';
  if (!opts) {
    opts = {};
  }
  if (typeof opts === 'function') {
    opts = {after: opts};
  }
  if (!opts.after) {
    opts.after = function (req, res) {
      res.json({message: 'logged out'});
    };
  }
  var cfg = auth.configuration;
  return function (req, res, injected) {
    if (typeof opts.before === 'function') {
      opts.before(req, res, injected);
    }
    if (req.session) {
      req.session.delete();
    }
    if (cfg.autoCreateSession) {
      req.session = auth.getSessionStorage().create();
    } else {
      if (cfg.type === 'cookie') {
        res.cookie(cfg.cookieName, '', {
          ttl: -(7 * 24 * 60 * 60),
          sign: cfg.cookieSecret ? {
            secret: cfg.cookieSecret,
            algorithm: cfg.cookieAlgorithm
          } : undefined
        });
      }
      delete req.session;
    }
    if (typeof opts.after === 'function') {
      opts.after(req, res, injected);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     FOXX SESSIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

var sessionTypes = ['cookie', 'header'];

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function Sessions(opts) {
  'use strict';
  if (!opts) {
    opts = {};
  }
  if (!opts.type) {
    opts.type = 'cookie';
  }

  if (opts.type === 'cookie') {
    if (opts.cookieSecret && typeof opts.cookieSecret !== 'string') {
      throw new Error('Cookie secret must be a string or empty.');
    }
    if (opts.cookieName && typeof opts.cookieName !== 'string') {
      throw new Error('Cookie name must be a string or empty.');
    }
    if (!opts.cookieName) {
      opts.cookieName = 'sid';
    }
  } else if (opts.type === 'header') {
    if (opts.headerName && typeof opts.headerName !== 'string') {
      throw new Error('Header name must be a string or empty.');
    }
    if (!opts.headerName) {
      opts.headerName = 'X-Session-Id';
    }
  } else {
    throw new Error('Only the following session types are supported at this time: ' + sessionTypes.join(', '));
  }
  if (opts.jwt !== false) {
    if (opts.jwtVerify !== false) {
      opts.jwtVerify = true;
    }
    if (!opts.jwtSecret) {
      opts.jwtSecret = '';
      if (!opts.jwtAlgorithm) {
        opts.jwtAlgorithm = 'none';
      } else if (opts.jwtAlgorithm !== 'none') {
        throw new Error('Must provide a JWT secret to use any algorithm other than "none".');
      }
    } else {
      opts.jwt = true;
      if (typeof opts.jwtSecret !== 'string') {
        throw new Error('Header JWT secret must be a string or empty.');
      }
      if (!opts.jwtAlgorithm) {
        opts.jwtAlgorithm = 'HS256';
      } else {
        opts.jwtAlgorithm = crypto.jwtCanonicalAlgorithmName(opts.jwtAlgorithm);
      }
    }
  }
  if (opts.autoCreateSession !== false) {
    opts.autoCreateSession = true;
  }
  if (!opts.sessionStorageApp) {
    opts.sessionStorageApp = '/_system/sessions';
  }
  this.configuration = opts;
}

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
/// @brief fetches the session storage
////////////////////////////////////////////////////////////////////////////////

Sessions.prototype.getSessionStorage = function () {
  'use strict';
  return Foxx.requireApp(this.configuration.sessionStorageApp).sessionStorage;
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

function UnauthorizedError(message) {
  'use strict';
  this.message = message;
  this.statusCode = 401;
}

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

exports.UnauthorizedError = UnauthorizedError;
exports.sessionTypes = sessionTypes;
exports.Sessions = Sessions;
exports.decorateController = decorateController;
exports.createDestroySessionHandler = createDestroySessionHandler;


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
