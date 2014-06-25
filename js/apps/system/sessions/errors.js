/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, es5: true */
/*global require, exports */
(function () {
  'use strict';

  function SessionNotFound(sid) {
    this.message = 'Session with session id ' + sid + ' not found.';
  }
  SessionNotFound.prototype = new Error();

  function SessionExpired(sid) {
    this.message = 'Session with session id ' + sid + ' has expired.';
  }
  SessionExpired.prototype = Object.create(SessionNotFound.prototype);

  exports.SessionNotFound = SessionNotFound;
  exports.SessionExpired = SessionExpired;
}());