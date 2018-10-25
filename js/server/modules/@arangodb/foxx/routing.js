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
const accepts = require('accepts');
const fs = require('fs');
const ansiHtml = require('ansi-html');
const dd = require('@arangodb/util').dedent;
const arangodb = require('@arangodb');
const ArangoError = arangodb.ArangoError;
const errors = arangodb.errors;
const actions = require('@arangodb/actions');
const routeLegacyService = require('@arangodb/foxx/legacy/routing').routeService;
const codeFrame = require('@arangodb/util').codeFrame;

function solarize (ansiText) {
  try {
    ansiHtml.setColors({
      reset: ['657b83', 'fdf6e3'],
      black: 'eee8d5',
      red: 'dc322f',
      green: '859900',
      yellow: 'b58900',
      blue: '268bd2',
      magenta: 'd33682',
      cyan: '2aa198',
      lightgrey: '586e75',
      darkgrey: '93a1a1'
    });
    const html = ansiHtml(ansiText);
    return html;
  } finally {
    ansiHtml.reset();
  }
}

function escapeHtml (raw) {
  return String(raw)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;');
}

function createErrorRoute (service, error, body, title) {
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
        callback (req, res) {
          res.responseCode = actions.HTTP_SERVICE_UNAVAILABLE;
          if (accepts(req).type('json', 'html') !== 'html') {
            const body = {
              error: true,
              errorNum: service.isDevelopment && error.errorNum || errors.ERROR_HTTP_SERVICE_UNAVAILABLE.code,
              errorMessage: service.isDevelopment && error.errorMessage || errors.ERROR_HTTP_SERVICE_UNAVAILABLE.message,
              code: service.isDevelopment && error.statusCode || res.responseCode
            };
            if (service.isDevelopment) {
              body.exception = String(error);
              body.stacktrace = error.stack.replace(/\n+$/, '').split('\n');
            }
            res.contentType = 'application/json';
            res.body = JSON.stringify(body);
            return;
          }
          res.contentType = 'text/html; charset=utf-8';
          res.body = dd`
            <!doctype html>
            <html>
              <head>
                <meta charset="utf-8">
                <title>${escapeHtml(title || 'Service Unavailable')}</title>
                <style>
                  body {
                    box-sizing: border-box;
                    margin: 0 auto;
                    padding: 0 2em;
                    max-width: 1200px;
                    font-family: sans-serif;
                    font-size: 14pt;
                    font-weight: 200;
                    line-height: 1.5;
                    background-color: #eff0f3;
                    color: #333;
                  }
                  h1 {
                    font-size: 31.5px;
                  }
                  h2 {
                    font-size: 24.5px;
                  }
                  h1, h2, p, pre {
                    margin: 1em 0;
                  }
                  * + h1, * + h2 {
                    margin-top: 2em;
                  }
                </style>
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
  return createErrorRoute(service, new ArangoError({
    errorNum: errors.ERROR_SERVICE_NEEDS_CONFIGURATION.code,
    errorMessage: errors.ERROR_SERVICE_NEEDS_CONFIGURATION.message
  }), dd`
    <h1>Service needs to be configured</h1>
    <p>
      This service requires configuration
      and has either not yet been fully configured
      or is missing some of its dependencies.
    </p>
  `);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief routes this service if the original is broken service
// //////////////////////////////////////////////////////////////////////////////

function createBrokenServiceRoute (service, err) {
  if (service.isDevelopment) {
    const title = String(err).replace(/\n/g, ' ');
    const frame = codeFrame(err.cause || err, service.basePath, true);
    let stacktrace = err.stack;
    while (err.cause) {
      err = err.cause;
      stacktrace += `\nvia ${err.stack}`;
    }
    const body = [
      dd`
        <h1>Failed to mount service at "${
          escapeHtml(service.mount)
        }"</h1>
        <p>
          An error occurred while mounting this service.<br>
          This usually indicates a problem with the service itself.
        </p>
        <style>
          pre {
            position: relative;
            box-sizing: border-box;
            border: 1px solid rgba(140,138,137,.25);
            border-radius: 2px;
            padding: 1em 0 0;
            max-width: 100%;
            overflow: auto;
            font-size: initial;
            background-color: #fdf6e3;
            color: #657b83;
          }
          pre.stack::after {
            content: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAFUAAAAyCAMAAAAa2meKAAAC9FBMVEUAAADhq3udPhr89%2FDcahfbdRfXZQ7Scin25uNcMRDacRbbdRfkmVDabRr9%2Bffxzq%2FbdBfabBfbcxfbdRf35NHvvprabxLVYBDZdB%2Fz08XbZxfhiT7bcxfbbRfghzfkj0rjl1LbdRfikEvpq3bccxfcbRfhkkleNBXcdRf12cPcdhbfgC%2FsvZvgejbbdhnbdBr9%2BfXbeyP12cHgcCjgfzfcZhZeMxHadBfbdibwyqbik0nefDlhNhbkk0jnoGRdMhFcMhLedRjZXgrghzj12L%2FccRTjijfln13lnV%2FheyJoQCLZaQ3qqnf77eHruIhZLw3%2B%2Bfj449K3Wxz14NvbaBfvzLmnZjDefiXdgi5dMxHehTZcMhHhfz7fikTbfCr99O3kmVTfgi3ZchjdeCfikkjacxfxyKPceDDjiUtdMxF%2BXULbdRffeTX00K7r5uLhiUPxtX701bfloWxZLw323sfwyJ%2FqqXXsrHPBsKP29OrbdRdaMBDcdRbnoF7uxqbcXxbZbhnrtpPjmVTcYxf77uNvSCq8cjDyzauqZyv88unvv5SoUxdbLw7bdRfbdRfbdBXinmXxzq3jhUPmpGbdcCv028bvwpjuwJVzRB1mNxLbZRbqtYVcMhPjnWfrupHmpGXehkOseE3ohTTUycDno2RLLRfvv5XdeRblmlfbdRfXbxnYaRnXbBnYcxnYaxnaZhjXcRjbYxfZaBnZZxjcYRfZdBj%2F%2F%2F%2FaZBjYcxj58fDbdBb69vX47ezXbhhcMhH7%2BvrcXhX58%2FLYbxjcdxzacxT58%2FX37u346uv25t3cfSrXbBbWaA%2F47e%2Fy0rXnpnDbeCbdeyDdZh3YbhnacBf35ubhklLhjEHZdBfWahbYcBL24c7wyKrlnmXehTbcfjXYcB3XZA%2FYbg788%2Bz569%2F12cPuvqXwxqDps4LornvfhjzdaiOwWxZxOhFYLxD128zkmV3ZbCG8YR7KYxilURfaZRXaYRWbSxWFRBTqroXmjVnVeTCMVyoIq3fcAAAAqXRSTlMAAwUH%2FfsVCf7r5UEuDv7%2B7I6EdmA2IBH%2B%2Fd%2FEv6GViW9pW1NLRz89NiMj%2Ffv7%2BPPy7erl4tDPuqqemJN%2BdHJqXllYVFNSSEc5OS0sJRQRDQz%2B%2Fv36%2Bfn58%2FHn4t3W09DIyMi5s7CopKKenYyJgXNpZl9dRDs7NC8sKysdGhj7%2Bfj38e%2Fr5%2BXj4%2BLg39%2FZ2NbV0tHNy8jHwry4rqiYjYqKe3h4cmZMSikd%2B07kTQAABRNJREFUWMOt1nVUU1EcB%2FA7BkpIqkgJCCpid7eUStnd3d3d3d0dj42xEc85YEM2ZEEMRqeApNit%2F3jfW73BGJvse87Oeefuns%2B57757f%2FcCvaRZxzvOQM8Zfd7e2HijPkWDMbf2Gie8TjQeqzeSZO5cwYl%2B%2BfJ1TOKkzvpCva9N4HCiX2FqzJQ2%2Bnl1M9MKFLWIlqpJXQ30gba3QrgoyuHI1QN6MDsvQRCEV4pyLDn6G2s7BI%2FYsrKEr7d5HYjIkpubXcCBakJMovH4Ji4nJ0SeKGZUbgqfn5wSk5jRxF1ggxDVUKZdKt3uQ0LGnCahtoiqCkMPEQlEQuumqC3VqTBUqulu7ZVRTmaAmFUNqjSqNUm7L2NmxXLQdqw0GsXUVgu00xkJhdFBtW2nJjV8sk1jZnObKCqF4VB30wwgsPXUiLC2jdR3UzpE46Srm9zCZ5Rs95hbaVRf9NNgkl2n98qJ%2BB5nHTBm9aI%2Bnyb%2BqD0FZOlAmAI1KqOh0ZKGrhfQ6alzZs%2Fu2zeHRvny8z2b%2FUNZ7M2MFKw6NbK9mkFuHr547rR3dNgZ9o6lpn%2BuZbPZ7796E79idwWrTj1kTugb7O97b36WmMfjRUUhWGesd%2Fo7NkztxIeAmN1nFYNVo0Yql43X%2FF7duYrpYipV%2BPpfU7m9ZjoOHr6FpDgJFNVAndoWeMA%2Bno8XF%2FJ48n4sXm5JiSArs1AkhJ3Tc74YdmHBVizi6Quc1%2FmR8VlYKGfrqw6AvWLHJbHifRAul1udUpGHHR0J%2FMyQYtiZUl2IqITr6E4iwb0s%2B2j1VUPgMXLN0guWycmW8Jf8Nq%2BsFEXRsjx%2BdAJWjfn2%2BeXl5R%2B52DZi0mHiYSQSCWvagmEkYN6uu5Stq04GsgR6bvZqhWWD11OfKz2jLbCTIwZW4wzjSfYfsUnGRxQbQsVGT6GkSwQue2CtscJfEbYT1QY2QmDrQb35cLRJSUmJiRn2v7rQ66oQCO8xGG7r9vg8xKuoGkqtwXjfu%2FlZWdWC0JJcer2x4sC3vtug2xYfLkFldNJYXpZeFUt3pXoVCmHu2N0AZ5WqKdAc1ywewixMqaqsrEopyCwSiXLqqIy0rVh1NMImQa4aNlZkW6SKEfrBk8d6H%2B7Z0wIujryqomJVNbImGLt1YIOVqWHtQGOqoQBhugBSc%2FNmz%2F1brx%2FU2wItKxCpqJGrZSdEtlQNh2ijairCdCUW3TUn0PxiFbUP3o4gElyltQRaqN2yme6qbYPQtzSiGhkMYIwQGqbGa3M59jO8mM1srdq2xwItIqpx%2BP8IE043zUmru3HHsOXZzAC8Ui90uDnanAQ2zIMfreubuqo3Ekul7Ydvr5XKGM4S4E%2BhIeFxcfv6jW6OqROIahoZTmsPWNysmwHt4sNwZ82UnqxQhfmNYvlAVPtgR058iNMzoG3GpG1hOeJPbazC0roVpZTiakEOQR0CBkSFLNHl%2BurWbStrMPYwbsSNz6mxsUJRZj6qugZqFiGmNrZAlwxz9GW5QrO%2FiYnJVGkdEBZnviHu2C7nOgAd4%2BLihnQE4NEmj3FB2%2Bxk1UWo3Fvfa9xsdRMNPG6b%2FH0wzKiFvMFzrl28as1KuxwAdMzalUNH7jJw7hGorIzXp%2F6hCoU0TI2I%2BPZpuT8J6BoydpRuOn5UcaEZYcKe9cTT7f6QZQMHLhuyzg%2F8b4bOmBUkewzqbzJjhQFoekgrT4%2BUO9uP9B%2BxC%2Bgj5LVkxfOO7UCH%2FAN1Tj0N8jOwpAAAAABJRU5ErkJggg%3D%3D');
            position: absolute;
            right: 5px;
            bottom: 0;
            opacity: 0.75;
          }
        </style>
      `,
      frame ? dd`
        <pre>
        ${solarize(escapeHtml(frame))}
        </pre>
      ` : '',
      dd`
        <h2>Stacktrace</h2>
        <pre class="stack">
        ${escapeHtml(stacktrace)}
        </pre>
      `
    ].filter(Boolean).join('\n');
    return createErrorRoute(service, err, body, title);
  }
  return createErrorRoute(service, err, dd`
    <h1>Failed to mount service</h1>
    <p>
      An error occured while mounting the service.<br>
      Please check the log file for errors.
    </p>
  `);
}

function checkAndCreateDefaultDocumentRoute (service) {
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
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

// //////////////////////////////////////////////////////////////////////////////
// / @brief computes the routes and exports of an service
// //////////////////////////////////////////////////////////////////////////////

exports.routeService = function (service, throwOnErrors) {
  if (service.main.loaded && !service.isDevelopment) {
    return {
      exports: service.main.exports,
      routes: service.routes,
      docs: service.legacy ? null : service.docs
    };
  }

  if (service.needsConfiguration()) {
    return {
      get exports () {
        if (!throwOnErrors) {
          return service.main.exports;
        }
        throw new ArangoError({
          errorNum: errors.ERROR_SERVICE_NEEDS_CONFIGURATION.code,
          errorMessage: errors.ERROR_SERVICE_NEEDS_CONFIGURATION.message
        });
      },
      routes: createServiceNeedsConfigurationRoute(service),
      docs: null
    };
  }

  service._reset();

  let error = null;
  if (service.legacy) {
    error = routeLegacyService(service, throwOnErrors);
    checkAndCreateDefaultDocumentRoute(service);
  } else {
    checkAndCreateDefaultDocumentRoute(service);
    if (service.manifest.main) {
      try {
        service.main.exports = service.run(service.manifest.main);
      } catch (e) {
        e.codeFrame = codeFrame(e.cause || e, service.basePath);
        console.errorStack(
          e,
          `Service "${service.mount}" encountered an error while being mounted`
        );
        if (throwOnErrors) {
          throw e;
        }
        error = e;
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
