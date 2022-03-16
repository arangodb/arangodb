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

var wasmmodules = require("@arangodb/aql/wasmModules");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function WasmModulesSuite () {
  'use strict';

    const modulename = "my_module";
    const code = "AGFzbQEAAAABKwhgAAF/YAF/AX9gAn9/AX9gAABgAX8AYAN/f38Bf2ADf35/AX5gAn5+AX4CeQQWd2FzaV9zbmFwc2hvdF9wcmV2aWV3MQ5hcmdzX3NpemVzX2dldAACFndhc2lfc25hcHNob3RfcHJldmlldzEIYXJnc19nZXQAAhZ3YXNpX3NuYXBzaG90X3ByZXZpZXcxCXByb2NfZXhpdAAEA2VudgRtYWluAAIDCwoDBwMAAAAEAQEBBAUBcAECAgUGAQGAAoACBgkBfwFBoIjAAgsHeQkGbWVtb3J5AgADYWRkAAUZX19pbmRpcmVjdF9mdW5jdGlvbl90YWJsZQEABl9zdGFydAAGEF9fZXJybm9fbG9jYXRpb24ACAZmZmx1c2gADAlzdGFja1NhdmUACQxzdGFja1Jlc3RvcmUACgpzdGFja0FsbG9jAAsJBwEAQQELAQQKmQMKAwABCwcAIAAgAXwLBwAQBxACAAuNAQEEfyMAQRBrIgAkAAJAIAAiAkEMaiAAQQhqEABFBEACf0EAIAIoAgwiAUUNABogACABQQJ0IgNBE2pBcHFrIgAiASQAIAEgAigCCEEPakFwcWsiASQAIAAgA2pBADYCACAAIAEQAQ0CIAIoAgwLIAAQAyEAIAJBEGokACAADwtBxwAQAgALQccAEAIACwUAQYAICwQAIwALBgAgACQACxAAIwAgAGtBcHEiACQAIAALZwEBfyAABEAgACgCTEF/TARAIAAQDQ8LIAAQDQ8LQZAIKAIABEBBkAgoAgAQDCEBC0GMCCgCACIABEADQCAAKAJMGiAAKAIUIAAoAhxLBEAgABANIAFyIQELIAAoAjgiAA0ACwsgAQtpAQJ/AkAgACgCFCAAKAIcTQ0AIABBAEEAIAAoAiQRBQAaIAAoAhQNAEF/DwsgACgCBCIBIAAoAggiAkkEQCAAIAEgAmusQQEgACgCKBEGABoLIABBADYCHCAAQgA3AxAgAEIANwIEQQAL";
    
return {

    tearDown : function () {
	wasmmodules.unregister(modulename);
	// Keep in mind that modules that are once loaded into the runtime (by trying to execute a function from it)
	// will be in the runtime forever, they cannot be deleted from there. This will change with PREG-108.
    },

    test_modules_are_initially_empty : function () {
	assertEqual(wasmmodules.toArray(), []);
    },

    test_module_can_be_registered : function () {
	assertEqual(wasmmodules.register(modulename, code), {"installed": modulename});
    },

    test_names_of_all_registered_modules_are_shown : function () {
	wasmmodules.register(modulename, code);

	assertEqual(wasmmodules.toArray(), [modulename]);
    },

    test_module_with_invalid_base64_encoded_code_cannot_be_registered : function () {
	try {
	    wasmmodules.register(modulename, "AB$(");
	} catch (err) {
	    assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
	}
    },

    test_existing_module_is_overwritten_with_module_of_same_name : function () {
	wasmmodules.register(modulename, code, true);

	wasmmodules.register(modulename, "AQL/", false);

	assertEqual(wasmmodules.toArray(), [modulename]);
	assertEqual(wasmmodules.show(modulename), {
	    "result": {
		"name": modulename,
		"code": "AQL/",
		"isDeterministic": false
	    }});
    },

    test_a_registered_module_shows_its_detail_information : function () {
	wasmmodules.register(modulename, code);

	assertEqual(wasmmodules.show(modulename), {
	    "result": {
		"name": modulename,
		"code": code,
		"isDeterministic": false
	    }});
    },

    test_querying_a_unknown_module_gives_an_error : function () {
	try {
	    wasmmodules.show("unknown_module");
	} catch (err) {
	    assertEqual(err.errorNum, ERRORS.ERROR_BAD_PARAMETER.code);
	}
    },

    test_modules_can_be_unregistered : function () {
	wasmmodules.register(modulename, code);

	assertEqual(wasmmodules.unregister(modulename), {"removed": modulename});
    },

    test_modules_are_deleted_when_unregistered : function () {
	wasmmodules.register(modulename, code);

	wasmmodules.unregister(modulename);

	assertEqual(wasmmodules.toArray(), []);
    },

    test_function_in_registered_module_is_exectable_via_AQL_query : function () {
	wasmmodules.register(modulename, code);

	const queryresult = db._query("RETURN CALL_WASM('" + modulename + "', 'add', 1, 4)").toArray();

	assertEqual(queryresult, [ 5 ]);
    },

    test_unknown_module_execution_throws_error : function () {
	try {
	    const queryresult = db._query("RETURN CALL_WASM('unknown_module', 'add', 1, 4)").toArray();
	} catch (err) {
	    assertEqual(err.errorNum, ERRORS.ERROR_WASM_EXECUTION_ERROR.code);
	}
    },

    test_unknown_function_execution_throws_error : function () {
	wasmmodules.register(modulename, code);
	
	try {
	    const queryresult = db._query("RETURN CALL_WASM('" + modulename + "', 'function_not_inside_module', 1, 4)").toArray();
	} catch (err) {
	    assertEqual(err.errorNum, ERRORS.ERROR_WASM_EXECUTION_ERROR.code);
	}
    },

    test_wrong_code_cannot_be_executed : function () {
	wasmmodules.register("new_module", "AQL/");
	// TODO PREG-108 Use modulename to make sure that it is unregistered after the test
	// (cannot be done right, see reason in tearDown function)

	// TODO PREG-87 Return result from wasm3 parse_module function
	// and return ERROR_WASM_EXECUTION_ERROR in handler

	try {
	    const queryresult = db._query("RETURN CALL_WASM('new_module', 'function_not_inside_module', 1, 4)").toArray();
	} catch (err) {
	    assertEqual(err.errorNum, ERRORS.ERROR_INTERNAL.code);
	}
    },

  };
}

jsunity.run(WasmModulesSuite);

return jsunity.done();
