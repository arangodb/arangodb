require("../sinon_test.js");
require("../sinon/spy_test.js");
require("../sinon/call_test.js");
require("../sinon/stub_test.js");
require("../sinon/mock_test.js");
require("../sinon/util/fake_timers_test.js");
require("../sinon/collection_test.js");
require("../sinon/sandbox_test.js");
require("../sinon/assert_test.js");
require("../sinon/test_test.js");
require("../sinon/test_case_test.js");
require("../sinon/match_test.js");
var buster = require("../runner");

buster.testRunner.onCreate(function (runner) {
    runner.on("suite:end", function (results) {
        // Reporter will be set up after delay, allow
        // it to finish before we exit the process
        process.nextTick(function () {
            process.exit(results.ok ? 0 : 1);
        });
    });
});
