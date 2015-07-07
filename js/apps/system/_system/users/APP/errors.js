'use strict';
class UserNotFound extends Error {
  constructor(uid) {
    super();
    this.name = this.constructor.name;
    this.message = `User with user id ${uid} not found.`;
    Error.captureStackTrace(this, this.constructor);
  }
}

class UsernameNotAvailable extends Error {
  constructor(username) {
    super();
    this.name = this.constructor.name;
    this.message = `The username ${username} is not available or already taken.`;
    Error.captureStackTrace(this, this.constructor);
  }
}

exports.UserNotFound = UserNotFound;
exports.UsernameNotAvailable = UsernameNotAvailable;
