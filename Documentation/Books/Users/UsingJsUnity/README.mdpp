!CHAPTER Using jsUnity and node-jscoverage


!SUBSECTION jsUnity

The ArangoDB contains a wrapper for [jsUnity](http://jsunity.com/), a
lightyweight universal JavaScript unit testing framework.

!SUBSECTION Running jsUnity Tests

Assume that you have a test file containing

    function aqlTestSuite () {
      return {
  testSizeOfTestCollection : function () {
    assertEqual(5, 5);
      };
    }

    jsUnity.run(aqlTestSuite);

    return jsunity.done();

Then you can run the test suite using *jsunity.runTest*

    unix> ju.runTest("test.js");
    2012-01-28T19:10:23Z [10671] INFO Running aqlTestSuite
    2012-01-28T19:10:23Z [10671] INFO 1 test found
    2012-01-28T19:10:23Z [10671] INFO [PASSED] testSizeOfTestCollection
    2012-01-28T19:10:23Z [10671] INFO 1 test passed
    2012-01-28T19:10:23Z [10671] INFO 0 tests failed
    2012-01-28T19:10:23Z [10671] INFO 1 millisecond elapsed

!SUBSECTION Running jsUnity Tests with Coverage

You can use the coverage tool
[node-jscoverage](https://github.com/visionmedia/node-jscoverage)

Assume that your file live in a directory called `lib`. Use

    node-jscoverage lib lib-cov

to create a copy of the JavaScript files with coverage information.  Start the
ArangoDB with these files and use *jsunity.runCoverage* instead of
*jsunity.runTest*.

