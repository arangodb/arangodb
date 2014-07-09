/*jslint indent: 2, nomen: true, maxlen: 120, es5: true */
/*global require, exports, applicationContext */
(function () {
  'use strict';
  var _ = require('underscore'),
    arangodb = require('org/arangodb'),
    db = arangodb.db,
    Foxx = require('org/arangodb/foxx'),
    errors = require('./errors'),
    User = Foxx.Model.extend({}, {
      attributes: {
        user: {type: 'string', required: true},
        authData: {type: 'object', required: true},
        userData: {type: 'object', required: true}
      }
    }),
    users = new Foxx.Repository(
      applicationContext.collection('users'),
      {model: User}
    );

  function resolve(username) {
    var user = users.firstExample({user: username});
    if (!user.get('_key')) {
      return null;
    }
    return user;
  }

  function listUsers() {
    return users.collection.all().toArray().map(function (user) {
      return user.user;
    }).filter(Boolean);
  }

  function createUser(username, userData) {
    if (!userData) {
      userData = {};
    }
    if (!username) {
      throw new Error('Must provide username!');
    }
    var user;
    db._executeTransaction({
      collections: {
        read: [users.collection.name()],
        write: [users.collection.name()]
      },
      action: function () {
        if (resolve(username)) {
          throw new errors.UsernameNotAvailable(username);
        }
        user = new User({
          user: username,
          userData: userData,
          authData: {}
        });
        users.save(user);
      }
    });
    return user;
  }

  function getUser(uid) {
    var user;
    try {
      user = users.byId(uid);
    } catch (err) {
      if (
        err instanceof arangodb.ArangoError
          && err.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
      ) {
        throw new errors.UserNotFound(uid);
      }
      throw err;
    }
    return user;
  }

  function deleteUser(uid) {
    try {
      users.removeById(uid);
    } catch (err) {
      if (
        err instanceof arangodb.ArangoError
          && err.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
      ) {
        throw new errors.UserNotFound(uid);
      }
      throw err;
    }
    return null;
  }

  _.extend(User.prototype, {
    save: function () {
      var user = this;
      users.replace(user);
      return user;
    },
    delete: function () {
      try {
        deleteUser(this.get('_key'));
        return true;
      } catch (e) {
        if (e instanceof errors.UserNotFound) {
          return false;
        }
        throw e;
      }
    }
  });

  exports.resolve = resolve;
  exports.list = listUsers;
  exports.create = createUser;
  exports.get = getUser;
  exports.delete = deleteUser;
  exports.errors = errors;
  exports.repository = users;
}());