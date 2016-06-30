'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2013-2014 triAGENS GmbH, Cologne, Germany
// / Copyright 2015-2016 ArangoDB GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');
const dd = require('dedent');
const arangodb = require('@arangodb');
const actions = require('@arangodb/actions');
const routeLegacyService = require('@arangodb/foxx/legacy/routing').routeService;

function escapeHtml (raw) {
  return String(raw)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;');
}

function createErrorRoute (service, body, title) {
  return {
    urlPrefix: '',
    name: `foxx("${service.mount}")`,
    routes: [{
      internal: true,
      url: {
        match: '/*',
        methods: actions.ALL_METHODS
      },
      action: {
        callback(req, res) {
          res.responseCode = actions.HTTP_SERVICE_UNAVAILABLE;
          res.contentType = 'text/html; charset=utf-8';
          res.body = dd`
            <!doctype html>
            <html>
              <head>
                <meta charset="utf-8">
                <title>${title || 'Service Unavailable'}</title>
              </head>
              <body>
                ${body}
              </body>
            </html>
          `;
        }
      }
    }],
    middleware: [],
    context: {},
    models: {},
    foxx: true
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief routes a default Configuration Required service
// //////////////////////////////////////////////////////////////////////////////

function createServiceNeedsConfigurationRoute (service) {
  return createErrorRoute(service, dd`
      <p>This service is not configured.</p>
      ${service.isDevelopment && `
      <h3>Configuration Information</h3>
      <pre>${escapeHtml(JSON.stringify(service.getConfiguration(), null, 2))}</pre>
    `}
  `);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief routes this service if the original is broken service
// //////////////////////////////////////////////////////////////////////////////

function createBrokenServiceRoute (service, err) {
  if (service.isDevelopment) {
    return createErrorRoute(service, dd`
      <h3>Stacktrace</h3>
      <pre>${escapeHtml(err.stack)}</pre>
    `, escapeHtml(err));
  }
  return createErrorRoute(service, dd`
    <p>
      This service is temporarily not available.
      Please check the log file for errors.
    </p>
  `);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

// //////////////////////////////////////////////////////////////////////////////
// / @brief computes the routes and exports of an service
// //////////////////////////////////////////////////////////////////////////////

exports.routeService = function (service, throwOnErrors) {
  if (service.needsConfiguration()) {
    return {
      exports: service.main.exports,
      routes: createServiceNeedsConfigurationRoute(service),
      docs: null
    };
  }

  if (!service.isDevelopment && service.main.loaded) {
    return {
      exports: service.main.exports,
      routes: service.routes,
      docs: service.legacy ? null : service.docs
    };
  }

  service._reset();

  const defaultDocument = service.manifest.defaultDocument;
  if (defaultDocument) {
    service.routes.routes.push({
      url: {match: '/'},
      action: {
        do: '@arangodb/actions/redirectRequest',
        options: {
          permanently: false,
          destination: defaultDocument,
          relative: true
        }
      }
    });
  }

  let error = null;
  if (service.legacy) {
    error = routeLegacyService(service, throwOnErrors);
  } else {
    if (service.manifest.main) {
      try {
        service.main.exports = service.run(service.manifest.main);
      } catch (e) {
        console.errorLines(`Cannot execute Foxx service at ${service.mount}: ${e.stack}`);
        error = e;
        if (throwOnErrors) {
          throw e;
        }
      }
    }
    service.buildRoutes();
  }

  if (service.manifest.files) {
    const files = service.manifest.files;
    _.each(files, function (file, path) {
      const directory = file.path || file;
      const normalized = arangodb.normalizeURL(`/${path}`);
      const route = {
        url: {match: `${normalized}/*`},
        action: {
          do: '@arangodb/actions/pathHandler',
          options: {
            root: service.root,
            path: fs.join(service.path, directory),
            type: file.type,
            gzip: Boolean(file.gzip)
          }
        }
      };
      service.routes.routes.push(route);
    });
  }

  service.main.loaded = true;

  return {
    exports: service.main.exports,
    routes: error ? createBrokenServiceRoute(service, error) : service.routes,
    docs: service.legacy ? null : service.docs
  };
};
