/*jshint globalstrict:false, strict:false, maxlen: 200, unused: false*/
/*global assertEqual, assertTrue, assertFalse, fail, AQL_WARNING */

/* unused for functions with 'what' parameter.*/
////////////////////////////////////////////////////////////////////////////////
/// @brief test the AQL user functions management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
var ERRORS = arangodb.errors;
let pu = require('@arangodb/testutils/process-utils');

var wasmmodules = require("@arangodb/aql/wasmModules");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function WasmModulesSuite() {
	'use strict';

	const mock_name = "my_module";
	const mock_code = "AGFzbQEAAAABKwhgAAF/YAF/AX9gAn9/AX9gAABgAX8AYAN/f38Bf2ADf35/AX5gAn5+AX4CeQQWd2FzaV9zbmFwc2hvdF9wcmV2aWV3MQ5hcmdzX3NpemVzX2dldAACFndhc2lfc25hcHNob3RfcHJldmlldzEIYXJnc19nZXQAAhZ3YXNpX3NuYXBzaG90X3ByZXZpZXcxCXByb2NfZXhpdAAEA2VudgRtYWluAAIDCwoDBwMAAAAEAQEBBAUBcAECAgUGAQGAAoACBgkBfwFBoIjAAgsHeQkGbWVtb3J5AgADYWRkAAUZX19pbmRpcmVjdF9mdW5jdGlvbl90YWJsZQEABl9zdGFydAAGEF9fZXJybm9fbG9jYXRpb24ACAZmZmx1c2gADAlzdGFja1NhdmUACQxzdGFja1Jlc3RvcmUACgpzdGFja0FsbG9jAAsJBwEAQQELAQQKmQMKAwABCwcAIAAgAXwLBwAQBxACAAuNAQEEfyMAQRBrIgAkAAJAIAAiAkEMaiAAQQhqEABFBEACf0EAIAIoAgwiAUUNABogACABQQJ0IgNBE2pBcHFrIgAiASQAIAEgAigCCEEPakFwcWsiASQAIAAgA2pBADYCACAAIAEQAQ0CIAIoAgwLIAAQAyEAIAJBEGokACAADwtBxwAQAgALQccAEAIACwUAQYAICwQAIwALBgAgACQACxAAIwAgAGtBcHEiACQAIAALZwEBfyAABEAgACgCTEF/TARAIAAQDQ8LIAAQDQ8LQZAIKAIABEBBkAgoAgAQDCEBC0GMCCgCACIABEADQCAAKAJMGiAAKAIUIAAoAhxLBEAgABANIAFyIQELIAAoAjgiAA0ACwsgAQtpAQJ/AkAgACgCFCAAKAIcTQ0AIABBAEEAIAAoAiQRBQAaIAAoAhQNAEF/DwsgACgCBCIBIAAoAggiAkkEQCAAIAEgAmusQQEgACgCKBEGABoLIABBADYCHCAAQgA3AxAgAEIANwIEQQAL";
  const add_name = "add_module";
  const add_path = pu.TOP_DIR + "/tests/js/client/shell/add.wasm";
  const ipv6_name = "ipv6_module";
  const ipv6_path = pu.TOP_DIR + "/tests/js/client/shell/ipv6.wasm";

	return {

	        tearDownAll: function() {
	                wasmmodules.unregister(mock_name);
		  wasmmodules.unregister(add_name);
		  wasmmodules.unregister(ipv6_name);
	        },

		test_modules_are_initially_empty: function() {
		        assertEqual(wasmmodules.toArray(), []);
		},

		test_module_can_be_registered: function() {
			assertEqual(wasmmodules.register(mock_name, mock_code), { "installed": mock_name });
		},

		test_module_can_be_registered_via_file: function() {
			assertEqual(wasmmodules.registerByFile(add_name, add_path), { "installed": mock_name });
		},

		test_names_of_all_registered_modules_are_shown: function() {
			wasmmodules.register(mock_name, mock_code);

			assertEqual(wasmmodules.toArray(), [mock_name]);
		},

		test_module_with_invalid_base64_encoded_code_cannot_be_registered: function() {
			try {
				wasmmodules.register(mock_name, "AB$(");
			} catch (err) {
				assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
			}
		},

		test_existing_module_is_overwritten_with_module_of_same_name: function() {
			wasmmodules.register(mock_name, mock_code, true);

			wasmmodules.register(mock_name, "AQL/", false);

			assertEqual(wasmmodules.toArray(), [mock_name]);
			assertEqual(wasmmodules.show(mock_name), {
				"result": {
					"name": mock_name,
					"code": "AQL/",
					"isDeterministic": false
				}
			});
		},

		test_a_registered_module_shows_its_detail_information: function() {
			wasmmodules.register(mock_name, mock_code);

			assertEqual(wasmmodules.show(mock_name), {
				"result": {
					"name": mock_name,
					"code": mock_code,
					"isDeterministic": false
				}
			});
		},

		test_querying_an_unknown_module_gives_an_error: function() {
			try {
				wasmmodules.show("unknown_module");
			} catch (err) {
				assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
			}
		},

		test_modules_can_be_unregistered: function() {
			wasmmodules.register(mock_name, mock_code);

			assertEqual(wasmmodules.unregister(mock_name), { "removed": mock_name });
		},

		test_modules_are_deleted_when_unregistered: function() {
			wasmmodules.register(mock_name, mock_code);

			wasmmodules.unregister(mock_name);

			assertEqual(wasmmodules.toArray(), []);
		},

		test_unregistering_an_unknown_modules_throws_error: function() {
			try {
				wasmmodules.unregister("unknown_module");
			} catch (err) {
				assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
			}
		},

	        // test_wasm_module_needs_allocate_and_deallocate_function_to_be_executable_via_AQL: function() {
		//   wasmmodules.registerByFile(add_name, add_path);
		//   try {
		// 	db._query("RETURN CALL_WASM('" + add_name + "', 'add', 1.1, 4.5)").toArray();
		//   } catch (err) {
		// 	assertEqual(err.errorNum, ERRORS.ERROR_WASM_EXECUTION_ERROR.code);
		//   }
	        // },

		test_function_in_registered_module_is_exectable_via_AQL_query: function() {
			wasmmodules.registerByFile(add_name, add_path);

			const queryresult = db._query("RETURN CALL_WASM('" + add_name + "', 'add', 1.1, 4.1)").toArray();

			assertEqual(queryresult, [5.2]);
		},

		test_function_in_registered_module_is_exectable_via_AQL_query_on_DB_server: function() {
			wasmmodules.registerByFile(add_name, add_path);
			db._createDocumentCollection("newtable");
			db._query("INSERT {a: 1, b: 2} in newtable")

			const queryresult = db._query("FOR d in newtable RETURN CALL_WASM('" + add_name + "', 'add', d.a, d.b)").toArray();

			assertEqual(queryresult, [5]);

			db._drop("newtable");
		},

		test_unknown_module_execution_throws_error: function() {
			try {
				db._query("RETURN CALL_WASM('unknown_module', 'add', 1.1, 4.5)").toArray();
			} catch (err) {
				assertEqual(err.errorNum, ERRORS.ERROR_WASM_EXECUTION_ERROR.code);
			}
		},

		test_unknown_function_execution_throws_error: function() {
			wasmmodules.register(mock_name, mock_code);

			try {
				db._query("RETURN CALL_WASM('" + mock_name + "', 'function_not_inside_module', 1.1, 4.5)").toArray();
			} catch (err) {
				assertEqual(err.errorNum, ERRORS.ERROR_WASM_EXECUTION_ERROR.code);
			}
		},

	        test_function_can_be_executed_on_more_complicated_data_structures: function() {
			wasmmodules.registerByFile(ipv6_name, ipv6_path);

			const queryresult = db._query("RETURN CALL_WASM('" + ipv6_name + "', 'ipv6_string_to_array', '2001:0db8:85a3:08d3:1319:8a2e:0370:7344')").toArray();

			assertEqual(queryresult, [[32, 1,  13,  184, 133, 163, 8,   211,
                                      19, 25, 138, 46,  3,   112, 115, 68]]);
		},

	  test_expected_error_in_wasm_function_gives_error_when_called: function () {
		  wasmmodules.registerByFile(add_name, add_path);
		  try {
			db._query("RETURN CALL_WASM('" + add_name + "', 'add', 'some string', 4)").toArray();
		  } catch (err) {
			assertEqual(err.errorNum, ERRORS.ERROR_WASM_EXECUTION_ERROR.code);
		  }
	        },

	  // test_unexpected_error_in_wasm_function_gives_error_when_called: function() {
	  // 	  wasmmodules.registerByFile(mock_name, add_path);
	  // 	  try {
	  // 		db._query("RETURN CALL_WASM('" + mock_name + "', 'incorrect_string_function', 'some string')").toArray();
	  // 	  } catch (err) {
	  // 		assertEqual(err.errorNum, ERRORS.ERROR_WASM_EXECUTION_ERROR.code);
	  // 	  }
	  //       },

	};
}

jsunity.run(WasmModulesSuite);

return jsunity.done();
