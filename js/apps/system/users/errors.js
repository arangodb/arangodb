/*global exports */
(function () {
  'use strict';
  function UserNotFound(uid) {
    this.message = 'User with user id ' + uid + ' not found.';
    var err = new Error(this.message);
    err.name = this.constructor.name;
    this.stack = err.stack;
  }
  UserNotFound.prototype = new Error();

  function UsernameNotAvailable(username) {
    this.message = 'The username ' + username + ' is not available or already taken.';
    var err = new Error(this.message);
    err.name = this.constructor.name;
    this.stack = err.stack;
  }
  UsernameNotAvailable.prototype = new Error();

  exports.UserNotFound = UserNotFound;
  exports.UsernameNotAvailable = UsernameNotAvailable;
}());