'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

exports.createNativeRequest = function (opts) {
  const req = {
    requestType: opts.method || 'get',
    protocol: opts.protocol || 'http',
    database: opts.db || '_system',
    url: `${opts.mount || ''}${opts.path || ''}${opts.search || ''}`,
    suffix: opts.path ? opts.path.split('/').filter(Boolean) : [],
    headers: opts.headers || {},
    cookies: opts.cookies || {},
    portType: opts.socketPath ? 'unix' : (opts.protocol || 'http'),
    server: opts.server || {
        address: '198.51.100.1',
        port: opts.port || 8529,
        endpoint: opts.socketPath ? `unix://${opts.socketPath}` : null
    },
    client: opts.client || {
        address: '203.0.113.1',
        port: 33333
    }
  };
  if (opts.body) {
    req[Symbol.for('@arangodb/request.rawBodyBuffer')] = (
      opts.body instanceof Buffer ? opts.body : new Buffer(
        typeof opts.body !== 'string'
          ? JSON.stringify(opts.body, null, 2)
          : opts.body
      )
    );
  }
  return req;
};
