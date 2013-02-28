Using jsUnity and node-jscoverage{#jsUnity}
===========================================

@NAVIGATE_jsUnity
@EMBEDTOC{jsUnityTOC}

jsUnity{#jsUnityIntro}
======================

The ArangoDB contains a wrapper for <a href="http://jsunity.com/">jsUnity</a>, a
lightyweight universal JavAScript unit testing framework.

Running jsUnity Tests{#jsUnityRunningTest}
==========================================

Assume that you have a test file containing

    function aqlTestSuite () {
      return {
	testSizeOfTestCollection : function () {
	  assertEqual(5, 5);
      };
    }

    jsUnity.run(aqlTestSuite);

    return jsunity.done();

Then you can run the test suite using @FN{jsunity.runTest}

    unix> ju.runTest("test.js");
    2012-01-28T19:10:23Z [10671] INFO Running aqlTestSuite
    2012-01-28T19:10:23Z [10671] INFO 1 test found
    2012-01-28T19:10:23Z [10671] INFO [PASSED] testSizeOfTestCollection
    2012-01-28T19:10:23Z [10671] INFO 1 test passed
    2012-01-28T19:10:23Z [10671] INFO 0 tests failed
    2012-01-28T19:10:23Z [10671] INFO 1 millisecond elapsed

Running jsUnity Tests with Coverage{#jsUnityRunningCoverage}
============================================================

You can use the coverage tool <a
href="https://github.com/visionmedia/node-jscoverage">@LIT{node-jscoverage}</a>.

Assume that your file live in a directory called `lib`. Use

    node-jscoverage lib lib-cov

to create a copy of the JavaScript files with coverage information.  Start the
ArangoDB with these files and use @FN{jsunity.runCoverage} instead of
@FN{jsunity.runTest}.
