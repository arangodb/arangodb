/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, assertNotEqual, assertMatch */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dan Larkin-York
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");

const db = require("internal").db;
const request = require("@arangodb/request");
const url = require('url');
const _ = require("lodash");
const errors = require("internal").errors;
const getCoordinatorEndpoints = require('@arangodb/test-helper').getCoordinatorEndpoints;

const servers = getCoordinatorEndpoints();

function PregelSuite() {
	'use strict';
	let cs = [];
	let coordinators = [];
	const baseUrl = `/_api/control_pregel`;

	function sendRequest(method, endpoint, body, usePrimary) {
		let res;
		const i = usePrimary ? 0 : 1;
		try {
			const envelope = {
				json: true,
				method,
				url: `${coordinators[i]}${endpoint}`
			};
			if (method !== 'GET') {
				envelope.body = body;
			}
			res = request(envelope);
		} catch (err) {
			console.error(`Exception processing ${method} ${endpoint}`, err.stack);
			return {};
		}

		if (typeof res.body === "string") {
			if (res.body === "") {
				res.body = {};
			} else {
				res.body = JSON.parse(res.body);
			}
		}
		return res;
	}

	return {
		setUp: function () {
			coordinators = getCoordinatorEndpoints();
			if (coordinators.length < 2) {
				throw new Error('Expecting at least two coordinators');
			}

			cs = [];
			cs.push(db._create("vertices", { numberOfShards: 8 }));
			cs.push(db._createEdgeCollection("edges", { shardKeys: ["vertex"], distributeShardsLike: "vertices", numberOfShards: 8 }));
			for (let i = 1; i <= 250; ++i) {
				cs[0].save({ _key: `v_${i}` });
				const edges = [];
				for (let j = 1; j < i; ++j) {
					edges.push({ _from: `vertices/v_${j}`, _to: `vertices/v_${i}` });
				}
				cs[1].save(edges);
			}

			require("internal").wait(5.0);
		},

		tearDown: function () {
			db._drop("edges");
			db._drop("vertices");
			cs = [];
			coordinators = [];
		},

		testPregelAllJobs: function () {
			const task = {
				algorithm: "pagerank",
				vertexCollections: ["vertices"],
				edgeCollections: ["edges"],
				params: {
					resultField: "result",
					store: false
				},
			};
			let ids = [];
			for (let i = 0; i < 5; ++i) {
				const postResult = sendRequest('POST', baseUrl, task, i % 2 === 0);

				assertFalse(postResult === undefined || postResult === {});
				assertEqual(postResult.status, 200);
				assertTrue(postResult.body > 1);
				const pid = postResult.body;

				// directly cancel pregel run (before it finishes)
				const deleteResult = sendRequest('DELETE', `${baseUrl}/${pid}`, {}, {}, false);
				assertFalse(deleteResult === undefined || deleteResult === {});
				assertFalse(deleteResult.body.error);
				assertEqual(deleteResult.status, 200);

				ids.push(pid);
			}
			assertEqual(5, ids.length);

			// check that pregel runs were created
			const sleepIntervalSeconds = 0.2;
			const maxWaitSeconds = 10;
			ids.forEach((pid) => {
				var wakeupsLeft = maxWaitSeconds / sleepIntervalSeconds;
				var statusResponse;
				do {
					require("internal").sleep(sleepIntervalSeconds);
					statusResponse = sendRequest('GET', `${baseUrl}/${pid}`, {}, false);
					assertFalse(statusResponse === undefined || statusResponse === {});
					if (wakeupsLeft-- === 0) {
						assertTrue(false, "Cannot retrieve status of pregel run, got response code " + statusResponse.status);
						return;
					}
				} while (statusResponse.status !== 200);
			});

			// request all pregel runs at once
			const getResult = sendRequest('GET', baseUrl, {}, false);
			assertEqual(getResult.status, 200);
			assertEqual(5, getResult.body.length);
			getResult.body.forEach((task) => {
				assertEqual("pagerank", task.algorithm);
				assertEqual(db._name(), task.database);
				assertNotEqual(-1, ids.indexOf(task.id));
			});

		},

		testPregelForwarding: function () {
			const task = {
				algorithm: "pagerank",
				vertexCollections: ["vertices"],
				edgeCollections: ["edges"],
				params: {
					resultField: "result"
				}
			};
			let result = sendRequest('POST', baseUrl, task, true);
			assertFalse(result === undefined || result === {});
			assertEqual(result.status, 200);
			assertTrue(result.body > 1);
			const taskId = result.body;

			result = sendRequest('DELETE', `${baseUrl}/${taskId}`, {}, {}, false);
			assertFalse(result === undefined || result === {});
			assertFalse(result.body.error);
			assertEqual(result.status, 200);

			// check that run existed
			const sleepIntervalSeconds = 0.2;
			const maxWaitSeconds = 10;
			let wakeupsLeft = maxWaitSeconds / sleepIntervalSeconds;
			var statusResponse;
			do {
				require("internal").sleep(sleepIntervalSeconds);
				statusResponse = sendRequest('GET', `${baseUrl}/${taskId}`, {}, false);
				assertFalse(statusResponse === undefined || statusResponse === {});
				if (wakeupsLeft-- === 0) {
					assertTrue(false, "Cannot retrieve status of pregel run, got response code " + statusResponse.status);
					return;
				}
			} while (statusResponse.status !== 200);
		},

		testPregelWrongId: function () {
			let url = baseUrl;
			const task = {
				algorithm: "pagerank",
				vertexCollections: ["vertices"],
				edgeCollections: ["edges"],
				params: {
					resultField: "result"
				}
			};
			let result = sendRequest('POST', url, task, true);
			assertFalse(result === undefined || result === {});
			assertEqual(result.status, 200);
			assertTrue(result.body > 1);
			const taskId = result.body;

			// kill using wrong id
			result = sendRequest('DELETE', `${baseUrl}/12345${taskId}`, {}, {}, true);
			assertEqual(result.status, 404);
			assertEqual(errors.ERROR_CURSOR_NOT_FOUND.code, result.body.errorNum);
			assertMatch(/cannot find target server/, result.body.errorMessage);

			result = sendRequest('DELETE', `${baseUrl}/12345${taskId}`, {}, {}, false);
			assertEqual(result.status, 404);
			assertEqual(errors.ERROR_CURSOR_NOT_FOUND.code, result.body.errorNum);
			assertMatch(/cannot find target server/, result.body.errorMessage);

			// check that run existed
			const sleepIntervalSeconds = 0.2;
			const maxWaitSeconds = 10;
			let wakeupsLeft = maxWaitSeconds / sleepIntervalSeconds;
			var statusResponse;
			do {
				require("internal").sleep(sleepIntervalSeconds);
				statusResponse = sendRequest('GET', `${baseUrl}/${taskId}`, {}, false);
				assertFalse(statusResponse === undefined || statusResponse === {});
				if (wakeupsLeft-- === 0) {
					assertTrue(false, "Cannot retrieve status of pregel run, got response code " + statusResponse.status);
					return;
				}
			} while (statusResponse.status !== 200);
		},

	};
}

jsunity.run(PregelSuite);

return jsunity.done();
