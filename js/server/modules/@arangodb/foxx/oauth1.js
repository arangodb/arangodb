/* eslint camelcase: 0 */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const assert = require('assert');
const url = require('url');
const parseUrl = url.parse;
const formatUrl = url.format;
const parseQuery = require('querystring').parse;
const request = require('@arangodb/request');
const crypto = require('@arangodb/crypto');

function nonceAndTime () {
  const oauth_timestamp = Math.floor(Date.now() / 1000);
  const oauth_nonce = crypto.genRandomAlphaNumbers(32);
  return {oauth_timestamp, oauth_nonce};
}

function normalizeUrl (requestUrl, parameters) {
  const urlParts = url.parse(requestUrl);
  const urlTokens = [urlParts.protocol, '//', urlParts.hostname];
  if (urlParts.port && (
    (urlParts.protocol === 'http:' && urlParts.port !== '80') ||
    (urlParts.protocol === 'https:' && urlParts.port !== '443')
  )) {
    urlTokens.push(':', urlParts.port);
  }
  urlTokens.push(urlParts.pathname);
  const params = [];
  if (urlParts.query) {
    const qs = parseQuery(urlParts.query);
    parameters = Object.assign(qs, parameters);
  }
  for (const key of Object.keys(parameters)) {
    const value = parameters[key] === undefined ? '' : String(parameters[key]);
    params.push(`${encodeURIComponent(key)}=${encodeURIComponent(value)}`);
  }
  return {
    url: urlTokens.join(''),
    params: params.sort().join('&')
  };
}

function createSignatureBaseString (method, normalizedUrl, normalizedParams) {
  return [
    method.toUpperCase(),
    encodeURIComponent(normalizedUrl),
    encodeURIComponent(normalizedParams)
  ].join('&');
}

function parse (str) {
  try {
    return JSON.parse(str);
  } catch (e) {
    if (e instanceof SyntaxError) {
      return parseQuery(str);
    }
    throw e;
  }
}

module.exports = function oauth1 (cfg) {
  if (!cfg) {
    cfg = {};
  }
  assert(cfg.requestTokenEndpoint, 'No Request Token URL specified');
  assert(cfg.authEndpoint, 'No User Authorization URL specified');
  assert(cfg.accessTokenEndpoint, 'No Access Token URL specified');
  assert(cfg.clientId, 'No Client ID specified');
  assert(cfg.clientSecret, 'No Client Secret specified');
  const oauth_consumer_key = cfg.clientId;
  const oauth_signature_method = cfg.signatureMethod || 'HMAC-SHA1';
  assert((
    oauth_signature_method === 'HMAC-SHA1' ||
    oauth_signature_method === 'PLAINTEXT'
  ), `Unsupported signature method: "${oauth_signature_method}"`);

  function createSignature (signatureBaseString, token_secret) {
    const key = `${cfg.clientSecret || ''}&${token_secret || ''}`;
    if (oauth_signature_method === 'PLAINTEXT') {
      return key;
    }
    const hex = crypto.hmac(key, signatureBaseString, 'sha1');
    return new Buffer(hex, 'hex').toString('base64');
  }

  function createRequest (method, url, opts, token_secret) {
    const normalized = normalizeUrl(
      url,
      Object.assign(
        nonceAndTime(),
        {
          oauth_consumer_key,
          oauth_signature_method,
          oauth_version: '1.0'
        },
        opts
      )
    );
    const signatureBaseString = createSignatureBaseString(
      'POST',
      normalized.url,
      normalized.params
    );
    const signature = createSignature(signatureBaseString, token_secret);
    normalized.params += `&oauth_signature=${encodeURIComponent(signature)}`;
    const queryParams = [];
    const authParams = [];
    if (cfg.realm) {
      authParams.push(`realm="${encodeURIComponent(cfg.realm)}"`);
    }
    for (const param of normalized.params.split('&')) {
      if (param.startsWith('oauth_')) {
        const [key, value] = param.split('=');
        authParams.push(`${key}="${value}"`);
      } else {
        queryParams.push(param);
      }
    }
    return {
      url: normalized.url,
      qs: queryParams.join('&'),
      headers: {
        accept: 'application/json',
        authorization: `OAuth ${authParams.join(', ')}`
      }
    };
  }

  return {
    fetchRequestToken (oauth_callback, opts) {
      assert(oauth_callback, 'No Callback URL specified');
      const req = createRequest(
        'POST',
        cfg.requestTokenEndpoint,
        Object.assign({oauth_callback}, opts)
      );
      const res = request.post(req);
      if (!res.body) {
        throw new Error(`OAuth provider returned empty response with HTTP status ${res.status}`);
      }
      return parse(res.body);
    },
    getAuthUrl (oauth_token, opts) {
      assert(oauth_token, 'No Request Token specified');
      const endpoint = parseUrl(cfg.authEndpoint);
      delete endpoint.search;
      endpoint.query = Object.assign(
        parseQuery(endpoint.query),
        opts,
        {oauth_token}
      );
      return formatUrl(endpoint);
    },
    exchangeRequestToken (oauth_token, oauth_verifier, opts) {
      assert(oauth_token, 'No Request Token specified');
      assert(oauth_verifier, 'No Verification Code specified');
      const req = createRequest(
        'POST',
        cfg.accessTokenEndpoint,
        Object.assign({oauth_token, oauth_verifier}, opts)
      );
      const res = request.post(req);
      if (!res.body) {
        throw new Error(`OAuth provider returned empty response with HTTP status ${res.status}`);
      }
      return parse(res.body);
    },
    fetchActiveUser (oauth_token, oauth_token_secret, opts) {
      assert(oauth_token, 'No Access Token specified');
      assert(oauth_token_secret, 'No Token Secret specified');
      if (!cfg.activeUserEndpoint) {
        return null;
      }
      const req = createRequest(
        'GET',
        cfg.activeUserEndpoint,
        Object.assign({oauth_token}, opts),
        oauth_token_secret
      );
      const res = request.get(req);
      if (!res.body) {
        throw new Error(`OAuth provider returned empty response with HTTP status ${res.status}`);
      }
      return parse(res.body);
    },
    createSignedRequest (method, url, parameters, oauth_token, oauth_token_secret) {
      assert(oauth_token, 'No Access Token specified');
      assert(oauth_token_secret, 'No Token Secret specified');
      return createRequest(
        method,
        url,
        Object.assign({oauth_token}, parameters),
        oauth_token_secret
      );
    }
  };
};
