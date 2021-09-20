# Tests
When creating tests, please bear in mind, that every test added will add additional time during the PR runs.

So every added test *will* slow down PR runs.

Please also note that test runs in the CI share the system with other parallel runs. 
So CI tests are not a place to max out I/O, memory, or such.

So please use *BULK* inserts to slimline the creation of your test data.
If you can use a common dataset for several tests, please use `setUpAll` and `tearDownAll` to reduce resource usage.
If splitting a testsuite between read only and writing tests is necessary for this, please do so.

### Testsuites
Testsuites live in `js/client/modules/@arangodb/testsuites/`. This folder is spidered during the startup of `testing.js` and all files in it are loaded automatically.
Please think twice whether you need to create a new testsuite or your test will fit into one of the existing ones. All test suites need to be registered within Oskar in order to be executed during CI tests.
The structure of each testsuite is as follows: 

```
const functionsDocumentation = {
  'shell_server': 'shell server tests',
};
const optionsDocumentation = [
  '   - `skipAql`: if set to true the AQL tests are skipped',
];

const testPaths = {
  'shell_server': [ tu.pathForTesting('common/shell'), tu.pathForTesting('server/shell') ],
};

function shellServer (options) {
  // spider all testcases, including twin paths in the enterprise directory:
  let testCases = tu.scanTestPaths(testPaths.shell_server, options);
  // split the testcases by `--testBuckets` parameter
  testCases = tu.splitBuckets(options, testCases);

  // spawn the SUT, execute all tests via the `tu.runThere` function
  // alternatives are:
  // - runThere (use javascript shell exec to run the test *in* the server
  // - runInLocalArangosh (run in the very same arangosh as testing.js)
  // - runInArangosh (spawn a new arangosh, collect results via result file, more frequent process spawning)
  // - runInRSpec (launch tests via ruby rspec)
  // - runInDriverTest implemented inside the testsuite - see testsuites/driver.js as example
  let rc = tu.performTests(opts, testCases, 'shell_client, tu.runThere, /* startStopHandlers*/ );
  // startStopHandlers can be implemented to add additional code during the startup / shutdown of the SUT:
  // - preStart - before starting the SUT, can be used to add additional params etc.
  // - postStart - invoked after the primary SUT is online. can be used to start other systems
  // - healthCheck - to be invoked after each test to check whether all is well
  // - preStop - just before the stop to do things that would hinder the shutdown
  // - postStop - after the stop to clean up things.

  // pass up eventual errors that should prohibit flushing the SUT data
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

// plugin hook. This is executed during loading of the modules.
exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  // add all used testpaths, so i.e. `auto` or `find` can find the suite for single `--test` files
  Object.assign(allTestPaths, testPaths);
  // Register the testsuite itself
  testFns['shell_server'] = shellServer;
  // enable it to be ran with `./scripts/unittest all` ; if not ommit - adds the [x] in help the list.
  defaultFns.push('shell_server');
  // if your testsuite has CLI parameters, specify them including the default param here:
  opts['skipAql'] = false;

  // 
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
```

### Existing Javascript tests

#### [permission] => tests/js/client/permissions / [server_parameters] => tests/js/client/server_parameters / [server_permissions] => client/server_permissions
This set of testsuites is here to revalidate startup parameters of arangod processes.

The testfile persists of two parts, and the file is invoked twice: 

Controlling the startup parameters can be done with the `getOptions` part. In `server_permissions` this can also be used to setup data which may not be permitted during the later run:
```
if (getOptions === true) {
  return {
    'javascript.allow-port-testing': false
  };
}
```
The later part of the test may contain regular jsunity testsuites. 

If you want to revalidate a parameter that is common to arangosh & arangod, you *should* use a test in the `permission` testsuite, 
since spawning an arangosh requires way less resources than spawning a full arangodb setup.

If your testcase requires a setup with more permissions than the testcode itself, you should use `server_permissions`, since it will launch the SUT twice.

You should however *prefer* `server_parameters` which will launch the SUT only once with the parameters you've provided. 

You should try to use the `-[non]cluster` filename filters to also control whether your test is ran on cluster too to reduce computing power.

#### [dump*] to test dump and restore procedures
The dump family of testsuites lives in `js/client/modules/@arangodb/testsuites/dump.js`
and orchestrates a sequence:
- launching a SUT
- create test data within it
- dump this test data
- execute some manual sanity checks on the dump files
- restore it to a fresh SUT
- check data

Since many create & check functions should be common to *all* suites, the default behavior is to enable them, and only exclude them if unsupported.

The individual files to be used are controlled by code in the testsuite like this:

```
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-encrypted.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    foxxTest: 'check-foxx.js'
  };
```

So `tests/js/server/dump/dump-setup[-cluster].js` will be invoked during setting up the first SUT.
It derives all its functions from `dump-setup-common.inc` and only invokes them in sequence.

Revalidating will be done using `dump[-cluster].js`, which will obtain all its test functions from `dump-test.inc`.

Testdata to be used during the first create etc can be put into `tests/js/server/dump`.


#### [communication, chaos] testsuite to launch parallel load
These testsuites are intended to launch several arangosh, in order to generate 



#### [driver | go | java | js] run driver tests against the local testsuite
These testsuites aim to integrate the respective drivers test with testing.js, 
so you can easily invoke them on your workstation.

# more to come...
#### shell_server
tests/js/server
tests/js/server/aql
tests/js/server/import
tests/js/server/recovery
tests/js/server/replication
tests/js/server/replication/fuzz
tests/js/server/replication/aql
tests/js/server/replication/random
tests/js/server/replication/ongoing
tests/js/server/replication/ongoing/frompresent
tests/js/server/replication/ongoing/global
tests/js/server/replication/ongoing/global/spec
tests/js/server/replication/static
tests/js/server/replication/sync
tests/js/server/backup
tests/js/server/agency-restart
tests/js/server/paths
tests/js/server/upgrade-data
tests/js/server/stress
tests/js/server/resilience
tests/js/server/resilience/move-view
tests/js/server/resilience/failover-view
tests/js/server/resilience/transactions
tests/js/server/resilience/failover
tests/js/server/resilience/move
tests/js/server/resilience/analyzers
tests/js/server/resilience/failover-failure
tests/js/server/resilience/sharddist
tests/js/server/export
tests/js/server/shell
tests/js/common/aql
tests/js/common/replication
tests/js/common/http
tests/js/common/shell
tests/js/client
tests/js/client/authentication
tests/js/client/permissions
tests/js/client/ldap
tests/js/client/server_secrets
tests/js/client/wal_cleanup
tests/js/client/restart
tests/js/client/assets
tests/js/client/assets/queuetest
tests/js/client/active-failover
tests/js/client/load-balancing
tests/js/client/communication
tests/js/client/chaos
tests/js/client/http
tests/js/client/agency
tests/js/client/replication2
tests/js/client/shell
tests/js/client/server_permissions
