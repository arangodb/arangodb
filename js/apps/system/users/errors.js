/*jslint indent: 2, nomen: true, maxlen: 120, es5: true */
/*global require, exports */
(function () {
  'use strict';

  function UserNotFound(uid) {
    this.message = 'User with user id ' + uid + ' not found.';
  }
  UserNotFound.prototype = new Error();

  function UsernameNotAvailable(username) {
    this.message = 'The username ' + username + ' is not available or already taken.';
  }
  UsernameNotAvailable.prototype = new Error();

  exports.UserNotFound = UserNotFound;
  exports.UsernameNotAvailable = UsernameNotAvailable;
}());