/*jshint globalstrict:false, strict:false */
/* global getOptions, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

// API version 1 is not in ApiVersion.h's supportedApiVersions yet, so it is
// normally rejected before any handler runs. The failure point must be armed
// before the server registers its routes at startup, hence the startup option.
if (getOptions === true) {
  return {
    'server.failure-point': 'ApiVersion::treatVersion1AsSupported',
  };
}

const jsunity = require('jsunity');
const { assertEqual, assertTrue } = jsunity.jsUnity.assertions;
const db = require('@arangodb').db;
const { errors } = require('@arangodb');
const isCluster = require('internal').isCluster();

const cn = 'UnitTestsCollectionApiVersion';

// Bodies that only the backwards compatibility layer of v0 accepts, with the
// error v1 reports for them.
const invalidBodies = {
  typeAsString: [{ type: 'edge' }, errors.ERROR_BAD_PARAMETER],
  typeUnknownNumber: [{ type: 4 }, errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID],
  wrongAttributeType: [{ waitForSync: 'yes' }, errors.ERROR_BAD_PARAMETER],
  unknownAttribute: [{ cid: '123' }, errors.ERROR_BAD_PARAMETER],
};

function CollectionCreateApiVersionSuite() {
  const create = (prefix, body) =>
    arango.POST_RAW(`${prefix}/_api/collection`, { name: cn, ...body });

  let suite = {
    tearDown: function () {
      db._drop(cn);
    },
  };

  for (const [name, [body, error]] of Object.entries(invalidBodies)) {
    suite[`testV0Accepts_${name}`] = function () {
      const res = create('', body);
      assertEqual(200, res.code, JSON.stringify(res));
    };

    suite[`testV1Rejects_${name}`] = function () {
      const res = create('/_arango/v1', body);
      assertEqual(400, res.code, JSON.stringify(res));
      assertEqual(error.code, res.parsedBody.errorNum, JSON.stringify(res));
    };
  }

  return suite;
}

function CollectionPropertiesApiVersionSuite() {
  const properties = (prefix) =>
    arango.GET(`${prefix}/_api/collection/${cn}/properties`);

  return {
    setUp: function () {
      db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testV0HidesReplicationFactorOnSingleServer: function () {
      const props = properties('');
      assertEqual(isCluster, props.hasOwnProperty('replicationFactor'),
                  JSON.stringify(props));
    },

    testV1ShowsReplicationFactor: function () {
      const props = properties('/_arango/v1');
      assertTrue(props.hasOwnProperty('replicationFactor'),
                 JSON.stringify(props));
    },
  };
}

jsunity.run(CollectionCreateApiVersionSuite);
jsunity.run(CollectionPropertiesApiVersionSuite);
return jsunity.done();
