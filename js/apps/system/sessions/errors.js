/*global exports */
(function () {
  'use strict';
  function SessionNotFound(sid) {
    this.message = 'Session with session id ' + sid + ' not found.';
    var err = new Error(this.message);
    err.name = this.constructor.name;
    this.stack = err.stack;
  }
  SessionNotFound.prototype = new Error();

  function SessionExpired(sid) {
    this.message = 'Session with session id ' + sid + ' has expired.';
    var err = new Error(this.message);
    err.name = this.constructor.name;
    this.stack = err.stack;
  }
  SessionExpired.prototype = Object.create(SessionNotFound.prototype);

  exports.SessionNotFound = SessionNotFound;
  exports.SessionExpired = SessionExpired;
}());