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

const dd = require('dedent');
const assert = require('assert');
const joi = require('joi');
const gqlSync = require('graphql-sync');
const createRouter = require('@arangodb/foxx/router');

const GRAPHIQL_VERSION = '0.7.1';

module.exports = function graphql (cfg) {
  assert(cfg, 'Must pass options for graphql');

  function getVariables (variables, res) {
    if (typeof variables !== 'string') {
      return variables;
    }
    try {
      return JSON.parse(variables);
    } catch (e) {
      res.throw(400, 'Variables are invalid JSON', e);
    }
  }

  const router = createRouter();

  router.get(handler, 'get')
  .queryParam('query', joi.string().optional())
  .queryParam('variables', joi.string().optional())
  .queryParam('operationName', joi.string().optional());

  router.post(handler, 'post')
  .body(['json', 'application/x-www-form-urlencoded', 'application/graphql'])
  .queryParam('query', joi.string().optional())
  .queryParam('variables', joi.string().optional())
  .queryParam('operationName', joi.string().optional());

  return router;

  function handler (req, res) {
    const options = typeof cfg === 'function' ? cfg(req, res) : cfg;
    const gql = options.graphql || gqlSync;

    const params = typeof req.body === 'string' ? {query: req.body} : req.body || {};
    const query = req.queryParams.query || params.query;
    const operationName = req.queryParams.operationName || params.operationName;
    const variables = getVariables(req.queryParams.variables || params.variables, res);
    const showGraphiQL = options.graphiql && canDisplayGraphiQL(req, params);

    let result = null;
    try {
      result = handleRequest(options, res);
    } catch (e) {
      res.status(500);
      result = {errors: [e]};
    }
    if (result && result.errors) {
      result.errors = result.errors.map(options.formatError || gql.formatError);
    }

    if (showGraphiQL) {
      res.set('content-type', 'text/html');
      res.body = renderGraphiQL({
        query,
        variables,
        operationName,
        result
      });
    } else {
      res.json(result, options.pretty);
    }

    function handleRequest () {
      if (!query) {
        if (showGraphiQL) {
          return null;
        }
        res.status(400);
        return {errors: [new Error('Must provide query string')]};
      }

      const source = new gql.Source(query, 'GraphQL request');
      let documentAST;
      try {
        documentAST = gql.parse(source);
      } catch (e) {
        res.status(400);
        return {errors: [e]};
      }

      const schema = options.schema || options;
      const validationErrors = gql.validate(schema, documentAST, (
        options.validationRules
        ? gql.specifiedRules.concat(options.validationRules)
        : gql.specifiedRules
      ));
      if (validationErrors.length > 0) {
        res.status(400);
        return {errors: validationErrors};
      }

      if (req.method === 'GET') {
        const operationAST = gql.getOperationAST(documentAST, operationName);
        if (operationAST && operationAST.operation !== 'query') {
          if (showGraphiQL) {
            return null;
          }
          res.status(405);
          res.set('allow', 'POST');
          return {errors: [
            new Error(`Can only perform a ${operationAST.operation} operation from a POST request`)
          ]};
        }
      }
      try {
        return gql.execute(
          schema,
          documentAST,
          options.rootValue,
          options.context,
          variables,
          operationName
        );
      } catch (e) {
        return {errors: [e]};
      }
    }
  }
};

function canDisplayGraphiQL (req, params) {
  const raw = params.raw !== undefined;
  return !raw && req.accepts(['json', 'html']) === 'html';
}

function safeSerialize (data) {
  return data ? JSON.stringify(data).replace(/\//g, '\\/') : null;
}

function renderGraphiQL (options) {
  const queryString = options.query;
  const variablesString = options.variables ? JSON.stringify(options.variables, null, 2) : null;
  const resultString = options.result ? JSON.stringify(options.result, null, 2) : null;
  const operationName = options.operationName;

  return dd`
    <!--
    The request to this GraphQL server provided the header "Accept: text/html"
    and as a result has been presented GraphiQL - an in-browser IDE for
    exploring GraphQL.
    If you wish to receive JSON, provide the header "Accept: application/json" or
    add "&raw" to the end of the URL within a browser.
    -->
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="utf-8" />
      <title>GraphiQL</title>
      <meta name="robots" content="noindex" />
      <style>
        html, body {
          height: 100%;
          margin: 0;
          overflow: hidden;
          width: 100%;
        }
      </style>
      <link href="//cdn.jsdelivr.net/graphiql/${GRAPHIQL_VERSION}/graphiql.css" rel="stylesheet" />
      <script src="//cdn.jsdelivr.net/fetch/0.9.0/fetch.min.js"></script>
      <script src="//cdn.jsdelivr.net/react/15.0.0/react.min.js"></script>
      <script src="//cdn.jsdelivr.net/react/15.0.0/react-dom.min.js"></script>
      <script src="//cdn.jsdelivr.net/graphiql/${GRAPHIQL_VERSION}/graphiql.min.js"></script>
    </head>
    <body>
      <script>
        // Collect the URL parameters
        var parameters = {};
        window.location.search.substr(1).split('&').forEach(function (entry) {
          var eq = entry.indexOf('=');
          if (eq >= 0) {
            parameters[decodeURIComponent(entry.slice(0, eq))] =
              decodeURIComponent(entry.slice(eq + 1));
          }
        });
        // Produce a Location query string from a parameter object.
        function locationQuery(params) {
          return '?' + Object.keys(params).map(function (key) {
            return encodeURIComponent(key) + '=' +
              encodeURIComponent(params[key]);
          }).join('&');
        }
        // Derive a fetch URL from the current URL, sans the GraphQL parameters.
        var graphqlParamNames = {
          query: true,
          variables: true,
          operationName: true
        };
        var otherParams = {};
        for (var k in parameters) {
          if (parameters.hasOwnProperty(k) && graphqlParamNames[k] !== true) {
            otherParams[k] = parameters[k];
          }
        }
        var fetchURL = locationQuery(otherParams);
        // Defines a GraphQL fetcher using the fetch API.
        function graphQLFetcher(graphQLParams) {
          return fetch(fetchURL, {
            method: 'post',
            headers: {
              'Accept': 'application/json',
              'Content-Type': 'application/json'
            },
            body: JSON.stringify(graphQLParams),
            credentials: 'include',
          }).then(function (response) {
            return response.text();
          }).then(function (responseBody) {
            try {
              return JSON.parse(responseBody);
            } catch (error) {
              return responseBody;
            }
          });
        }
        // When the query and variables string is edited, update the URL bar so
        // that it can be easily shared.
        function onEditQuery(newQuery) {
          parameters.query = newQuery;
          updateURL();
        }
        function onEditVariables(newVariables) {
          parameters.variables = newVariables;
          updateURL();
        }
        function onEditOperationName(newOperationName) {
          parameters.operationName = newOperationName;
          updateURL();
        }
        function updateURL() {
          history.replaceState(null, null, locationQuery(parameters));
        }
        // Render <GraphiQL /> into the body.
        ReactDOM.render(
          React.createElement(GraphiQL, {
            fetcher: graphQLFetcher,
            onEditQuery: onEditQuery,
            onEditVariables: onEditVariables,
            onEditOperationName: onEditOperationName,
            query: ${safeSerialize(queryString)},
            response: ${safeSerialize(resultString)},
            variables: ${safeSerialize(variablesString)},
            operationName: ${safeSerialize(operationName)},
          }),
          document.body
        );
      </script>
    </body>
    </html>
  `;
}
