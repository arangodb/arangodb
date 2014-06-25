/*jslint indent: 2, nomen: true, maxlen: 120, es5: true */
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

var Foxx = require('org/arangodb/foxx');

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
    if (cfg.type === 'cookie') {
      req.session = sessions.fromCookie(req, cfg.cookieName, cfg.cookieSecret);
      if (!req.session && cfg.autoCreateSession) {
        req.session = sessions.create();
      }
    }
  });

  controller.after('/*', function (req, res) {
    if (req.session) {
      if (cfg.type === 'cookie') {
        req.session.addCookie(res, cfg.cookieName, cfg.cookieSecret);
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
        req.session.clearCookie(res, cfg.cookieName, cfg.cookieSecret);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function Sessions(opts) {
  'use strict';
  if (!opts) {
    opts = {};
  }
  if (opts.type !== 'cookie') {
    throw new Error('Only "cookie" type sessions are supported at this time.');
  }
  if (opts.cookieSecret && typeof opts.cookieSecret !== 'string') {
    throw new Error('Cookie secret must be a string or empty.');
  }
  if (opts.cookieName && typeof opts.cookieName !== 'string') {
    throw new Error('Cookie name must be a string or empty.');
  }

  if (!opts.type) {
    opts.type = 'cookie';
  }
  if (!opts.cookieName) {
    opts.cookieName = 'sid';
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

exports.UnauthorizedError                   = UnauthorizedError;
exports.Sessions                      = Sessions;
exports.decorateController                  = decorateController;
exports.createDestroySessionHandler                 = createDestroySessionHandler;

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
