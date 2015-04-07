/*jshint globalstrict:true */
/*global exports */
'use strict';
function SessionNotFound(sid) {
  this.message = 'Session with session id ' + sid + ' not found.';
  this.name = this.constructor.name;
  Error.captureStackTrace(this, SessionNotFound);
}
SessionNotFound.prototype = new Error();
SessionNotFound.prototype.constructor = SessionNotFound;

function SessionExpired(sid) {
  this.message = 'Session with session id ' + sid + ' has expired.';
  this.name = this.constructor.name;
  Error.captureStackTrace(this, SessionExpired);
}
SessionExpired.prototype = Object.create(SessionNotFound.prototype);
SessionExpired.prototype.constructor = SessionExpired;

exports.SessionNotFound = SessionNotFound;
exports.SessionExpired = SessionExpired;