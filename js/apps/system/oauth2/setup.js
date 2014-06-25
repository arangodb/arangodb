/*jslint indent: 2, nomen: true, maxlen: 120, white: true, plusplus: true, unparam: true, regexp: true, vars: true */
/*global require, applicationContext */
(function () {
  'use strict';
  var db = require('org/arangodb').db;
  var providersName = applicationContext.collectionName('providers');

  if (db._collection(providersName) === null) {
    db._create(providersName);
  }

  var providers = db._collection(providersName);
  [
    {
      _key: 'github',
      label: 'GitHub',
      authEndpoint: 'https://github.com/login/oauth/authorize?scope=user',
      tokenEndpoint: 'https://github.com/login/oauth/access_token',
      activeUserEndpoint: 'https://api.github.com/user',
      clientId: null,
      clientSecret: null
    },
    {
      _key: 'facebook',
      label: 'Facebook',
      authEndpoint: 'https://www.facebook.com/dialog/oauth',
      tokenEndpoint: 'https://graph.facebook.com/oauth/access_token',
      activeUserEndpoint: 'https://graph.facebook.com/v2.0/me',
      clientId: null,
      clientSecret: null
    },
    {
      _key: 'google',
      label: 'Google',
      authEndpoint: 'https://accounts.google.com/o/oauth2/auth?access_type=offline&scope=profile',
      tokenEndpoint: 'https://accounts.google.com/o/oauth2/token',
      refreshEndpoint: 'https://accounts.google.com/o/oauth2/token',
      activeUserEndpoint: 'https://www.googleapis.com/plus/v1/people/me',
      clientId: null,
      clientSecret: null
    }
  ].forEach(function(provider) {
    if (!providers.exists(provider._key)) {
      providers.save(provider);
    }
  });
}());