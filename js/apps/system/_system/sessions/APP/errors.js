'use strict';
class SessionNotFound extends Error {
  constructor(sid) {
    super();
    this.name = this.constructor.name;
    this.message = `Session with session id ${sid} not found.`;
    Error.captureStackTrace(this, this.constructor);
  }
}

class SessionExpired extends SessionNotFound {
  constructor(sid) {
    super(sid);
    this.name = this.constructor.name;
    this.message = `Session with session id ${sid} has expired.`;
    Error.captureStackTrace(this, this.constructor);
  }
}

exports.SessionNotFound = SessionNotFound;
exports.SessionExpired = SessionExpired;