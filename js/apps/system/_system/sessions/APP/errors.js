'use strict';
function SessionNotFound(sid) {
  this.message = 'Session with session id ' + sid + ' not found.';
  this.name = this.name;
  Error.captureStackTrace(this, SessionNotFound);
}
SessionNotFound.prototype = new Error();
SessionNotFound.prototype.constructor = SessionNotFound;
Object.defineProperty(SessionNotFound.prototype, 'name', {
  enumerable: true,
  configurable: true,
  get: function () {
    return this.constructor.name;
  }
});

function SessionExpired(sid) {
  this.message = 'Session with session id ' + sid + ' has expired.';
  this.name = this.name;
  Error.captureStackTrace(this, SessionExpired);
}
SessionExpired.prototype = Object.create(SessionNotFound.prototype);
SessionExpired.prototype.constructor = SessionExpired;
SessionExpired.prototype.name = SessionExpired.name;
Object.defineProperty(SessionExpired.prototype, 'name', {
  enumerable: true,
  configurable: true,
  get: function () {
    return this.constructor.name;
  }
});

exports.SessionNotFound = SessionNotFound;
exports.SessionExpired = SessionExpired;