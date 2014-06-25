/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, es5: true */
/*global require, exports, applicationContext */
(function () {
  'use strict';
  var _ = require('underscore');
  var url = require('url');
  var querystring = require('querystring');
  var internal = require('internal');
  var arangodb = require('org/arangodb');
  var db = arangodb.db;
  var Foxx = require('org/arangodb/foxx');
  var errors = require('./errors');

  var Provider = Foxx.Model.extend({}, {
    attributes: {
      _key: {type: 'string', required: true},
      label: {type: 'string', required: true},
      authEndpoint: {type: 'string', required: true},
      tokenEndpoint: {type: 'string', required: true},
      refreshEndpoint: {type: 'string', required: false},
      activeUserEndpoint: {type: 'string', required: false},
      usernameTemplate: {type: 'string', required: false},
      clientId: {type: 'string', required: true},
      clientSecret: {type: 'string', required: true}
    }
  });

  var providers = new Foxx.Repository(
    applicationContext.collection('providers'),
    {model: Provider}
  );

  function listProviders() {
    return providers.collection.all().toArray().forEach(function (provider) {
      return _.pick(provider, '_key', 'label', 'clientId');
    });
  }

  function createProvider(data) {
    var provider = new Provider(data);
    providers.save(provider);
    return provider;
  }

  function getProvider(key) {
    var provider;
    try {
      provider = providers.byId(key);
    } catch (err) {
      if (
        err instanceof arangodb.ArangoError &&
          err.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
      ) {
        throw new errors.ProviderNotFound(key);
      } else {
        throw err;
      }
    }
    return provider;
  }

  function deleteProvider(key) {
    try {
      providers.removeById(key);
    } catch (err) {
      if (
        err instanceof arangodb.ArangoError
          && err.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
      ) {
        throw new errors.ProviderNotFound(key);
      } else {
        throw err;
      }
    }
    return null;
  }

  _.extend(Provider.prototype, {
    getAuthUrl: function (redirect_uri, args) {
      if (redirect_uri && typeof redirect_uri === 'object') {
        args = redirect_uri;
        redirect_uri = undefined;
      }
      var endpoint = url.parse(this.get('authEndpoint'));
      args = _.extend(querystring.parse(endpoint.query), args);
      if (redirect_uri) {
        args.redirect_uri = redirect_uri;
      }
      if (!args.response_type) {
        args.response_type = 'code';
      }
      args.client_id = this.get('clientId');
      endpoint.search = '?' + querystring.stringify(args);
      return url.format(endpoint);
    },
    _getTokenRequest: function (code, redirect_uri, args) {
      if (code && typeof code === 'object') {
        args = code;
        code = undefined;
        redirect_uri = undefined;
      } else if (redirect_uri && typeof redirect_uri === 'object') {
        args = redirect_uri;
        redirect_uri = undefined;
      }
      var endpoint = url.parse(this.get('tokenEndpoint'));
      args = _.extend(querystring.parse(endpoint.query), args);
      if (code) {
        args.code = code;
      }
      if (redirect_uri) {
        args.redirect_uri = redirect_uri;
      }
      if (!args.grant_type) {
        args.grant_type = 'authorization_code';
      }
      args.client_id = this.get('clientId');
      args.client_secret = this.get('clientSecret');
      delete endpoint.search;
      delete endpoint.query;
      return {url: url.format(endpoint), body: args};
    },
    getActiveUserUrl: function (access_token, args) {
      var endpoint = this.get('activeUserEndpoint');
      if (!endpoint) {
        return null;
      }
      if (access_token && typeof access_token === 'object') {
        args = access_token;
        access_token = undefined;
      }
      args = _.extend(querystring.parse(endpoint.query), args);
      if (access_token) {
        args.access_token = access_token;
      }
      endpoint = url.parse(endpoint);
      args = _.extend(querystring.parse(endpoint.query), args);
      endpoint.search = '?' + querystring.stringify(args);
      return url.format(endpoint);
    },
    getUsername: function (obj) {
      var tpl = this.get('usernameTemplate');
      if (!tpl) {
        tpl = '<%= id %>';
      }
      return _.template(tpl)(obj);
    },
    exchangeGrantToken: function (code, redirect_uri) {
      var request = this._getTokenRequest(code, redirect_uri);
      var response = internal.download(
        request.url,
        querystring.stringify(request.body),
        {
          method: 'POST',
          headers: {
            accept: 'application/json',
            'content-type': 'application/x-www-form-urlencoded'
          }
        }
      );
      if (!response.body) {
        throw new Error('OAuth provider ' + this.get('_key') + ' returned HTTP ' + response.code);
      }
      try {
        return JSON.parse(response.body);
      } catch (err) {
        if (err instanceof SyntaxError) {
          return querystring.parse(response.body);
        }
        throw err;
      }
    },
    fetchActiveUser: function (access_token) {
      var url = this.getActiveUserUrl(access_token);
      if (!url) {
        throw new Error('Provider ' + this.get('_key') + ' does not support active user lookup');
      }
      var response = internal.download(url);
      if (!response.body) {
        throw new Error('OAuth provider ' + this.get('_key') + ' returned HTTP ' + response.code);
      }
      try {
        return JSON.parse(response.body);
      } catch (err) {
        if (err instanceof SyntaxError) {
          return querystring.parse(response.body);
        }
        throw err;
      }
    },
    save: function () {
      var provider = this;
      providers.replace(provider);
      return provider;
    },
    delete: function () {
      try {
        deleteProvider(this.get('_key'));
        return true;
      } catch (e) {
        if (e instanceof errors.ProviderNotFound) {
          return false;
        }
        throw e;
      }
    }
  });

  exports.list = listProviders;
  exports.create = createProvider;
  exports.get = getProvider;
  exports.delete = deleteProvider;
  exports.errors = errors;
  exports.repository = providers;
}());