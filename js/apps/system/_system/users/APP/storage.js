/*global require, exports, applicationContext */
(function () {
  'use strict';
  var _ = require('underscore'),
    joi = require('joi'),
    arangodb = require('org/arangodb'),
    db = arangodb.db,
    Foxx = require('org/arangodb/foxx'),
    errors = require('./errors'),
    User = Foxx.Model.extend({
      schema: {
        user: joi.string().required(),
        authData: joi.object().required(),
        userData: joi.object().required()
      }
    }),
    users;

  if (applicationContext.mount.indexOf('/_system/') === 0) {
    users = new Foxx.Repository(
      db._collection('_users'),
      {model: User}
    );
  } else {
    users = new Foxx.Repository(
      applicationContext.collection('users'),
      {model: User}
    );
  }


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

  function createUser(username, userData, authData) {
    if (!userData) {
      userData = {};
    }
    if (!authData) {
      authData = {};
    }
    if (
      applicationContext.mount.indexOf('/_system/') === 0
        && !authData.hasOwnProperty('active')
    ) {
      authData.active = true;
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
          authData: authData
        });
        users.save(user);
      }
    });
    if (applicationContext.mount.indexOf('/_system/') === 0) {
      require('org/arangodb/users').reload();
    }
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
    if (applicationContext.mount.indexOf('/_system/') === 0) {
      require('org/arangodb/users').reload();
    }
    return null;
  }

  _.extend(User.prototype, {
    save: function () {
      var user = this;
      users.replace(user);
      if (applicationContext.mount.indexOf('/_system/') === 0) {
        require('org/arangodb/users').reload();
      }
      return user;
    },
    delete: function () {
      try {
        deleteUser(this.get('_key'));
      } catch (e) {
        if (e instanceof errors.UserNotFound) {
          return false;
        }
        throw e;
      }
      if (applicationContext.mount.indexOf('/_system/') === 0) {
        require('org/arangodb/users').reload();
      }
      return true;
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
