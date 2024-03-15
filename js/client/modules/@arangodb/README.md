=== Arango DB test infrasturcture ===

| Path / File                                                  | Description                                                                                                 |
| :----------------------------------------------------------- | :---------------------------------------------------------------------------------------------------------- |
| `testutils/testing.js`                                       | invoked via `unittest.js` handles module structure for `testsuites`.                                        |
| `testutils/test-utils.js`                                    | infrastructure for tests like filtering, bucketing, iterating                                               |
| `testutils/process-utils.js`                                 | manage arango instances, start/stop/monitor SUT-processes                                                   |
| `testutils/result-processing.js`                             | work with the result structures to produce reports, hit lists etc.                                          |
| `testutils/crash-utils.js`                                   | if something goes wrong, this contains the crash analysis tools                                             |
| `testutils/instance-manager.js`                              | manache a full installation be it cluster or single server consisting of one or several `instance.js`es.    |
| `testutils/instance.js`                                      | manage one arangod process - to be used via the `instance-manager.js`                                       |
| `testutils/testrunner.js`                                    | lifecycle of one testsuite.                                                                                 |
| `testutils/clusterstats.js`                                  | can be launched separately to monitor the cluster instances and their resource usage                        |
| `testsuites/`                                                | modules with testframework that control one set of tests each utilizing the `testrunner.js`.                |
| `testsuites/dump.js`                                         | complex infrastructure managing several instance lifecycles during serveral dump/restore/hotbackup attempts |
| `testsuites/[go|php|java|js|driver|rta_makedata].js`         | subsidize the lifecycle method of `testrunner.js` for other instance and testrunner controls.               |
| `testsuites/[server_permissions|recovery[_cluster]].js`      | run several instances and run individual tests on them; vary parameters etc. of SUT                         |
| `testsuites/chaos.js`                                        | launches several sub-arangoshs to cause randomness, enables failurepoints and so on.                        |
| `js/common/modules[/jsunity]/jsunity.js`                     | jsunity testing framework; invoked via jsunity.js next to the module                                        |
| `js/common/modules/@arangodb/mocha-runner.js`                | wrapper for running mocha tests in arangodb                                                                 |


=== code structure === 
