buster.util = buster.util || {};

if (!buster.util.testCase) {
    buster.util.testCase = function (name, tests) {
        var jstdTests = {};

        for (var test in tests) {
            if (test != "setUp" && test != "tearDown") {
                //testCase.prototype["test " + test] = tests[test];
                jstdTests["test " + test] = tests[test];
            } else {
                //testCase.prototype[test] = tests[test];
                jstdTests[test] = tests[test];
            }
        }

        if (typeof sinon != "undefined") {
            return TestCase(name, sinon.testCase(jstdTests));
        } else {
            return TestCase(name, jstdTests);
        }
    }
}

var assert = this;

(function () {
    var mappedAssertions = {
        doesNotThrow: "NoException",
        throws: "Exception"
    };

    for (var assertion in mappedAssertions) {
        assert[assertion] = assert["assert" + mappedAssertions[assertion]];
    }

    assert.ok = function (val) {
        assertTrue(!!val);
    };

    assert.deepEqual = assert.equal = function (expected, actual, message) {
        assertEquals(message, actual, expected);
    };
}());

if (typeof buster != "undefined" && buster.assert) {
    if (buster.format) {
        buster.assert.format = buster.format.ascii;
    }

    buster.assert.fail = fail;
}
