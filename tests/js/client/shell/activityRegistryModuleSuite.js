/* jshint esnext: true */
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
// //////////////////////////////////////////////////////////////////////////////
'use strict';
const activitiesModule = require('@arangodb/activity-registry');

function activityRegistryModuleSuite() {
    return {
        setUpAll: function () {
        },

        tearDownAll: function () {
        },

        testPrintsOutASingleActivity: function () {
            assertEqual(activitiesModule.pretty_print([
                {
                    "id": "0x7a1c8743c780",
                    "name": "ActivityRegistryRestHandler",
                    "state": "Active",
                    "parent": {},
                    "metadata": {
                        "method": "GET",
                        "url": "/_admin/activity-registry"
                    }
                }
            ]),
                'ActivityRegistryRestHandler: {"method":"GET","url":"/_admin/activity-registry"}');
        },
        testPrintsOutSeparateActivities: function () {
            const lines = activitiesModule.pretty_print([
                {
                    "id": "0x7a1c86f957c0",
                    "name": "dump context",
                    "state": "Active",
                    "parent": {
                        "id": "0x7a1c87025500"
                    },
                    "metadata": {
                        "database": "_system",
                        "user": "root",
                        "id": "dump-1855652990508072960"
                    }
                },
                {
                    "id": "0x7b9050e26140",
                    "name": "RestDumpHandler",
                    "state": "Active",
                    "parent": {},
                    "metadata": {
                        "method": "POST",
                        "url": "/_api/dump/start"
                    }
                }
            ]).split('\n');
            assertEqual(lines.length, 2);
            assertTrue(lines.includes('ActivityRegistryRestHandler: {"method":"GET","url":"/_admin/activity-registry"}'));
            assertTrue(lines.includes('ActivityDumpHandler: {"method":"POST","url":"/_api/dump/start"}'));
        },
    };
}
exports.activityRegistryModuleSuite = activityRegistryModuleSuite;
