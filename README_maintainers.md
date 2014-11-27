
ArangoDB Maintainers manual
---------------------------
---------------------------
---------------------------
This file contains documentation about the build process, documentation generation means, unittests - put short - if you want to hack parts of arangod this could be interesting for you.

Configure
---------
---------
--enable-relative
--enable-maintainer-mode
--enable-all-in-one-icu
--with-backtrace

CFLAGS
------
 -DDEBUG_CLUSTER_COMM - Add backtraces to cluster requests so you can easily track their origin

V8 Special flags:
-DENABLE_GDB_JIT_INTERFACE
(enable (broken) GDB intergation of JIT)
At runtime arangod needs to be started with these options:
--javascript.v8-options="--gdbjit_dump"
--javascript.v8-options="--gdbjit_full"


Runtime
-------
 * start arangod with --console to get a debug console
 * Cheapen startup for valgrind: --no-server --javascript.gc-frequency 1000000 --javascript.gc-interval 65536 --scheduler.threads=1 --javascript.v8-contexts=1
 * to have backtraces output set this on the prompt: ENABLE_NATIVE_BACKTRACES(true)

Documentation
-------------
-------------
Dependencies:
 * swagger
 * gitbook (https://github.com/GitbookIO/gitbook)
 * markdown-pp (https://github.com/triAGENS/markdown-pp)
 * cURL if you want to cut'n'paste execute the examples

Where...
--------
 - js/action/api/* - markdown comments in source with execution section
 - Documentation/Books/Users/SUMMARY.md - index of all sub documentations
 - Documentation/Scripts/generateSwaggerApi.py - list of all sections to be adjusted if

generate
--------
 - make swagger - on toplevel to generate the documentation interactively with the server
 - cd Documentation/Books; make - to generate the HTML documentation

read / use the documentation
----------------------------
 - file:///Documentation/Books/books/Users/index.html contains the generated documentation
 - JS-Console - Tools/API - Interactive documentation which you can play with.


JSLint
------
------
(we switched to jshint a while back - this is still named jslint for historical reasons)

Make target
-----------
use
make jslint
to find out whether all of your files comply to jslint. This is required to make contineous integration work smoothly.

if you want to add new files / patterns to this make target, edit js/Makefile.files

Use standalone for your js file
-------------------------------
If you want to search errors in your js file, jslint is very handy - like a compiler is for C/C++.
You can invoke it like this:

bin/arangosh --jslint js/server/modules/org/arangodb/testing.js


ArangoDB Unittesting Framework
------------------------------
------------------------------
Dependencies
------------
 * Ruby, rspec


Filename conventions
====================
Special patterns in filenames are used to select tests to be executed or skipped depending on parameters:

-cluster
--------
These tests will only run if clustering is tested. (option 'cluster' needs to be true).

-noncluster
-----------
These tests will only run if no cluster is used. (option 'cluster' needs to be false)

-timecritical
-------------
These tests are critical to execution time - and thus may fail if arangod is to slow. This may happen i.e. if you run the tests in valgrind, so you want to avoid them. To skip them, set the option 'skipTimeCritical' to true.

-disabled
---------
These tests are disabled. You may however want to run them by hand.

replication
-----------
(only applies to ruby tests)
These tests aren't run automaticaly since they require a manual set up environment.

-spec
-----
These tests are ran using the jasmine framework instead of jsunity.

Test frameworks used
====================
There are several major places where unittests live: 
 - UnitTests/HttpInterface
 - UnitTests/Basics
 - UnitTests/Geo
 - js/server/tests (runneable on the server)
 - js/common/tests (runneable on the server & via arangosh)
 - js/common/test-data
 - js/client/tests (runneable via arangosh)
 - /js/apps/system/aardvark/test


HttpInterface - RSpec Client Tests
---------------------------------
TODO: which tests are these?


jsUnity on arangod
------------------
you can engage single tests when running arangod with console like this:
require("jsunity").runTest("js/server/tests/aql-queries-simple.js");


jsUnity via arangosh
--------------------
arangosh is similar, however, you can only run tests which are intended to be ran via arangosh:
require("jsunity").runTest("js/client/tests/shell-client.js");



jasmine tests
-------------

Jasmine tests cover two important usecase:
  - testing the UI components of aardvark
-spec

aardvark





UnitTests/Makefile.unittests


Invocation methods
==================



Make-targets
------------
(used in travis CI integration)
Most of the tests can be invoked via the main Makefile:
 - unittests
  - unittests-brief
  - unittests-verbose
 - unittests-recovery
 - unittests-config
 - unittests-boost
 - unittests-single
 - unittests-shell-server
 - unittests-shell-server-only
 - unittests-shell-server-ahuacatl
 - unittests-shell-client-readonly
 - unittests-shell-client
 - unittests-http-server
 - unittests-ssl-server
 - unittests-import
 - unittests-replication
  - unittests-replication-server
  - unittests-replication-http
  - unittests-replication-data
 - unittests-upgrade
 - unittests-dfdb
 - unittests-foxx-manager
 - unittests-dump
 - unittests-arangob
 - unittests-authentication
 - unittests-authentication-parameters


Javascript framework
--------------------
(used in Jenkins integration)
Invoked like that:
scripts/run scripts/unittest.js all

calling it without parameters like this:
scripts/run scripts/unittest.js
will give you a extensive usage help which we won't duplicate here.

Choosing facility
_________________

The first parameter chooses the facility to execute.
Available choices include:
 - all: (calls multiple) This target is utilized by most of the jenkins builds invoking unit tests.
 - single_client: (see Running a single unittestsuite)
 
 - single_server: (see Running a single unittestsuite)
 - many more - call without arguments for more details.
     

Passing Options
_______________

Options are passed in as one json; Please note that formating blanks may cause problems.
Different facilities may take different options. The above mentioned usage output contains
the full detail.

A commandline for running a single test (-> with the facility 'single_server') using
valgrind could look like this: 

 scripts/run scripts/unittest.js single_server \
    '{"test":"js/server/tests/aql-escaping.js",'\
    '"extraargs":["--server.threads=1",'\
        '"--scheduler.threads=1",'\
        '"--javascript.gc-frequency","1000000",'\
        '"--javascript.gc-interval","65536"],'\
    '"valgrind":"/usr/bin/valgrind",'\
    '"valgrindargs":["--log-file=/tmp/valgrindlog.%p"]}'

 - we specify the test to execute
 - we specify some arangod arguments which increase the server performance
 - we specify to run using valgrind (this is supported by all facilities
 - we specify some valgrind commandline arguments

Running a single unittestsuite
------------------------------
Testing a single test with the frame work directly on a server:
scripts/run scripts/unittest.js single_server '{"test":"js/server/tests/aql-escaping.js"}'

Testing a single test with the frame work via arangosh:
scripts/run scripts/unittest.js single_client '{"test":"js/server/tests/aql-escaping.js"}'

unittest.js is mostly only a wrapper; The backend functionality lives in:
js/server/modules/org/arangodb/testing.js



arangod Emergency console
-------------------------

require("jsunity").runTest("js/server/tests/aql-escaping.js");

arangosh client
---------------

require("jsunity").runTest("js/server/tests/aql-escaping.js");


arangod commandline arguments
-----------------------------

bin/arangod /tmp/dataUT --javascript.unit-tests="js/server/tests/aql-escaping.js" --no-server


make unittest

js/common/modules/loadtestrunner.js
