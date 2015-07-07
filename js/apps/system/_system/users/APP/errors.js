'use strict';
function UserNotFound(uid) {
  this.message = 'User with user id ' + uid + ' not found.';
  var err = new Error(this.message);
  err.name = this.name;
  this.stack = err.stack;
}
UserNotFound.prototype = new Error();
UserNotFound.prototype.constructor = UserNotFound;
Object.defineProperty(UserNotFound.prototype, 'name', {
  enumerable: true,
  configurable: true,
  get: function () {
    return this.constructor.name;
  }
});

function UsernameNotAvailable(username) {
  this.message = 'The username ' + username + ' is not available or already taken.';
  var err = new Error(this.message);
  err.name = this.name;
  this.stack = err.stack;
}
UsernameNotAvailable.prototype = new Error();
UsernameNotAvailable.prototype.constructor = UsernameNotAvailable;
Object.defineProperty(UsernameNotAvailable.prototype, 'name', {
  enumerable: true,
  configurable: true,
  get: function () {
    return this.constructor.name;
  }
});

exports.UserNotFound = UserNotFound;
exports.UsernameNotAvailable = UsernameNotAvailable;
