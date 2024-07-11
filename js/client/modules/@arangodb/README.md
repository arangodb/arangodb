# Arango DB test infrastructure
## Philosophy
All the code structure and the test infrastructure works in a way
- to have results in all (worst-) cases
- that it must not lock up, and terminate in a reasonable timeframe under all circumstances
- to waste as little host resources as possible -> 
  - detect lockups and abort tests (C++ / JS infrastructure)
  - have a well established timeout and deadline mechanism 
  - have a subprocess monitoring to abort as fast as possible if spawned processes go away
    (i.e. clusters may get hell slow once instances are missing)
- to collect as much data from the system as possible
- to compress the data to present the user with a most compact result
- to have a Timeout for individual integration testsuites. Its 15 minutes by default.
- to have a Deadline for the total chunk of tests of one run. 

## files concerned

| Path / File                                                  | Description                                                                                                 |
| :----------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------- |
| `testutils/testing.js`                                       | invoked via `unittest.js` handles module structure for `testsuites`.                                        |
| `testutils/result-processing.js`                             | contains code to run various analyses on results and/or generate result representations for the user/ci     |
| `testutils/test-utils.js`                                    | infrastructure for tests like filtering, bucketing, iterating, RPC or eval launch                           |
| `testutils/process-utils.js`                                 | start/stop/monitor all of the SUT-sub-processes                                                             |
| `testutils/portmanager.js`                                   | finds free tcp ports for arangods to launch.                                                                |
| `testutils/crash-utils.js`                                   | if something goes wrong, this contains the crash analysis tools                                             |
| `testutils/client-tools.js`                                  | code to manage the arangodb client-tools for their respective tests                                         |
| `testutils/instance-manager.js`                              | manage a full installation, be it cluster or single server consisting of one or several `instance.js`es.    |
| `testutils/instance.js`                                      | manage one arangod process - to be used via the `instance-manager.js`                                       |
| `testutils/testrunner.js`                                    | base class to manage the lifecycle of one testsuite.                                                        |
| `testutils/testrunners.js`                                   | various derived classes of the lifecycle managager of `testrunne.js`.                                       |
| `testutils/clusterstats.js`                                  | can be launched separately to monitor the cluster instances and their resource usage                        |
| `testsuites/`                                                | modules with testframework that control one set of tests each utilizing the `testrunner.js`.                |
| `testsuites/dump.js`                                         | complex infrastructure managing several instance lifecycles during serveral dump/restore/hotbackup attempts |
| `testsuites/[go\|php\|java\|js\|driver\|rta_makedata].js`    | subsidize the lifecycle method of `testRunner` for other instance and testrunner controls.                  |
| `testsuites/[server_permissions\|recovery[_cluster]].js`     | run several instances and run individual tests on them; vary parameters etc. of SUT                         |
| `testsuites/chaos.js`                                        | launches several sub-arangoshs to cause randomness, enables failurepoints and so on.                        |
| `js/common/modules[/jsunity]/jsunity.js`                     | jsunity testing framework; invoked via jsunity.js next to the module                                        |
| `js/common/modules/@arangodb/mocha-runner.js`                | wrapper for running mocha tests in arangodb                                                                 |
| `client-tools/Shell/ProcessMonitoringFeature.[h\|cpp]`       | establishes a thread that will watch after all registered subprocesses. Pull the plug to abort asap.        |
| `client-tools/Shell/v8-deadline.cpp`                         | infrastructure to implement a deadline which if engaged aborts all I/O + subprocesses immediately.          |

## Deadline / Process monitoring
The deadline handling is here so all http subrequests and `waitpid()`s are adjusted to end at this point in time. A busy polling is implemented to abort if the process monitor signals SUT-unstability.
The js infrastructure code will manage the deadline so test code must not exceed certain timeouts and ultimately abort the test execution on the deadline. 
The js code will then terminate the SUT and aggregate its process states. This even moreso happenes if a crash of a subprocess leaves behind a cordeump.

### code structure
The `testing.js` API conists of 3 main vectors, on top of it `testing.js` chooses which tests to run according to commandline options and how to present the test results.
The initial behaviour is different for users in their local development environment and the CI. 
`--extremeVerbosity true` can be specified to closer follow & debug which way the infrastructure is working.

The suite API:

## module integration
- testsuite name, paired with documentation
- optional commandline parameters to be handled via `config`.
- a list of files associated with the testsuites; This is used for the `auto` mechanism to associate a testfile with a suite.
- `exports.setup` function to:
  - register the suites directories containing integration testsuites with `allTestPaths` to to the testsuite.
  - register the functions of the testsuites in `testFns`
  - register the testsuite documentations with `fnDocs`

### testsuite run integration
The suite function. It should launch the SUT, identify & select the integration testsuites to run, shutdown the SUT and return the results.
To achieve this it should lean on the various infrastructure presented below.

### result integration
The result should be in a structure that can later be analyzed using `result-processing.js`.

### test infrastructure
The test infrastructure should be used by testsuites to do most of the heavy lifting; in best case it can contain just a few lines.

### integration testsuites management
A testsuite receives a list of testfiles where each one is a single `integration testsuite`. Its files located by `testing.js` from the `allTestPaths` directory list.
Testing.js already filtered it, and split it to buckets (test-utils.js:`filterTestcaseByOptions()`) according to commandline options.

The testsuite then can use the default lifecycle management to launch these tests, or implement own logic, again best based on existing logic.

## test lifecycle management
The test lifecycle management should be utilized to maintain the SUT, iterate over the tests. 
It is implemented in `testrunner.js`, and its modularity is intended to reuse it partially or completely.
By default it comes without a way to launch an integration testsuite, this is implemented in the deriving classes. 

An important part of the `testRunner` is its API to register tests that ensure SUT-cleanness after individual integration testsuites.
This way it is maintained, that individual integration testsuites don't leave things behind which may have influence on subsequent integration testsuites. 
It consists of: 
- `name` of the ensurer
- `setUp` to be ran before the integration testsuite is launched to establish the "desired" dataset 
- `runCheck` to be ran after the integration testsuite to check the SUT has the same state as before.
Both `setUp` and `runCheck` can indicate the SUT is not fit for further tests, and abort the execution.

The object manages its subprocesses in the `instanceManager` and uses it to manipulate processes.
The API consists of several default callback hooks that can be implemented dubbed `pre*()`/`post*()`.
- `healthCheck` is to be ran to ping the system and check whether its fit to be inspected for cleanness, or running 
- `runOneTest` must be implemented to handle the invocation of one integration testsuite.
  Several descendant implementations are available:
   - `tu.runOnArangodRunner` to launch testcode per remote execution within the arangod SUT
   - `tu.runInArangoshRunner` to launch an isolated sub arangosh to launch the test
   - `tu.runLocalInArangoshRunner` to launch the testsuites in the very same arangosh, being able to i.e. use the `instanceManager` of the `testRunner` to manipulate the instance.
- `run` - the main loop it will:
  - launch the SUT via `instanceManager` and run certain pre/post hooks
  - loop over the integration testsuites provided
    - invoke hooks
    - collect system-pre-states via `setUp` of the ensurer modules
    - if `forceTerminate` was registered on a previous run, abort. 
    - launch the next integration testsuite via the `runOneTest` integration point
    - collect states of the SUT via netstat, or `/proc`, memory accounting interface
    - establish that the SUT is healthy
    - establish that all `runCheck`s are right
  - invoke all the pre/post shutdown hooks
  - make `instanceManager` stop the SUT
  - create & return the testresult structure

Tests that work more outside of the default `testRunner` code by replacing more of it:
- `rta_makedata.js` - `rtaMakedataRunner` will implement its own `runOneTest` hook which a method that runs all phases of the test in one go; hence the phases and results don't match directly into the structure.
- `chaos.js` - `chaosRunner` will launch the SUT, and run a set of arangoshs with parallel test executions on the SUT, and collect them in the end.
- `[go|php|java|js|driver].js` will replace the `run` hook by launching the SUT on their own, and running  respective test-suites of these drivers, providing them with their instance, and convert their respective test-results back to `testing.js` own format.
- `[server_permissions|recovery[_cluster]].js` will also replace the `run` function. It will launch one SUT per iteration and run its respective tests with it.

## instance management
An arangodb deployment may consist of several instances that are to be orchestrated in order to form the SUT. 
Depending on the `options` the deployment mode is chosen by the `instanceManager`.
It forms the `instance` objects of the respective `arangod`s to be launched and stopped.
It knows the concepts of the agency and how to work with it.
It knows all tcp ports and can launch sniffing utilitise to track the tests.
It knows all PIDs and should be utilized to manipulate them. 
It knows how to establish when the SUT is launched, and to maintain whether its healthy or not.
It knows via its `instance`s which processes to start, stop, halt or maybe restart.

## dump integration tests
`dump.js` doesn't use the test lifecycle management in `testrunner.js`. It leans on `instanceManager` for SUT handling, and `client-tools.js` to launch client-tools -binaries.

Each phase of the dump suite will result in an aequivilant test status, that can individually fail.
Once a failure occurs all subsequent test execution will be skipped, since they lean on proper preparation by the previous tests.
### files concerned

| Path / File                                                  | Description                                                                                                 |
| :----------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------- |
| `testsuites/dump.js`                                         | complex infrastructure managing several instance lifecycles during serveral dump/restore/hotbackup attempts |
| `tests/js/client/dump/`                                      | test logic for all variations of deployments distributed over several files                                 |
| `[dump-singleserver\|dump-multiple\|dump-cluster]`           | previously created dumps to be restored within the tests                                                    |
| `dump-setup-common.inc`                                      | contains all functions to create test data to create before creating the dump                               |
| `dump-test.inc`                                              | contains all functions to test data after restore                                                           |
| `dump-setup-*js`                                             | consists of calls to the respective set from `dump-setup-common.inc` of test data to be created             |
| `dump-check*js`                                              | consists of calls to the respective set from `dump-setup-common.inc` of the test data to be checked         |
| `check*`                                                     | more checks that could be referenced from individual dump suites                                            |

### structure in `tests/js/client/dump/` files
To create the individual initial provisioning dataset, `setup .js` files invoke the respective list of setup functions from `dump-setup-common.inc`.

To create the individual set of tests, the `dump-test.inc` files testsuide `deriveTestSuite()` is used by the file to create its respective subset.

Individual `check` files can be registered by tests in `dump.js`.

In general its probably beneficial to rather consider creating a `rta-makedata` testsuite than work on the above structure. 

### structure in `testsuites/dump.js`
`dump.js` consists of 4 layers:
- `DumpRestoreHelper` to abstract the invocation of individual client-tools, commandline argument handling, result handling and test phases. 
- `dump_backend_two_instances` function orchestrating the phases using the `DumpRestoreHelper` depending on provided input structures
- i.e. `dumpMixedClusterSingle` are individual `testing.js`-testsuites to create a list of checks to run, and orchestrate its sequence
- regular testing.js testsuite interface registering of these testsuite-functions

The individual testsuites (3) generate a set of parameters that create phases of instances to be launched, stuffed, dumped, stopped, launch subsequent fresh instance, restore into it, cleanup the data.

