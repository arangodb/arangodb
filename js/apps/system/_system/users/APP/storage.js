'use strict';
const _ = require('underscore');
const joi = require('joi');
const arangodb = require('@arangodb');
const db = arangodb.db;
const Foxx = require('@arangodb/foxx');
const errors = require('./errors');
const refreshUserCache = require('@arangodb/users').reload;
const User = Foxx.Model.extend({
  schema: {
    user: joi.string().required(),
    authData: joi.object().required(),
    userData: joi.object().required()
  }
});

function getCollection() {
  return db._collection('_users');
}

function resolve(username) {
  const user = getCollection().firstExample({user: username});
  if (!user) {
    return null;
  }
  return new User(user);
}

function listUsers() {
  return getCollection().all().toArray().map(function (user) {
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
  if (!authData.active) {
    authData.active = true;
  }

  if (!username) {
    throw new Error('Must provide username!');
  }

  const collection = getCollection();
  let user;
  db._executeTransaction({
    collections: {
      read: [collection.name()],
      write: [collection.name()]
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
      const meta = collection.save(user.attributes);
      user.set(meta);
    }
  });
  refreshUserCache();
  return user;
}

function getUser(uid) {
  let user;
  try {
    user = getCollection().document(uid);
  } catch (err) {
    if (
      err instanceof arangodb.ArangoError
        && err.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
    ) {
      throw new errors.UserNotFound(uid);
    }
    throw err;
  }
  return new User(user);
}

function deleteUser(uid) {
  try {
    getCollection().remove(uid);
  } catch (err) {
    if (
      err instanceof arangodb.ArangoError
        && err.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
    ) {
      throw new errors.UserNotFound(uid);
    }
    throw err;
  }
  refreshUserCache();
  return null;
}

_.extend(User.prototype, {
  save: function () {
    const user = this;
    const meta = getCollection().replace(user.attributes, user.attributes);
    user.set(meta);
    refreshUserCache();
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
    refreshUserCache();
    return true;
  }
});

exports.resolve = resolve;
exports.list = listUsers;
exports.create = createUser;
exports.get = getUser;
exports.delete = deleteUser;
exports.errors = errors;
