
ArangoDB Maintainers manual
===========================

This file contains documentation about the build process, documentation generation means, unittests - put short - if you want to hack parts of arangod this could be interesting for you.

CMake
=====

Essentially, you can compile ArangoDB from source by issueing the
following commands from a clone of the source repository:

    mkdir build
    cd build
    cmake ..
    make
    cd ..

After that, the binaries will reside in `build/bin`. To quickly start
up your compiled ArangoDB, simply do

    build/bin/arangod -c etc/relative/arangod.conf data

This will use a configuration file that is included in the source
repository.

CMake flags
-----------
 * *-DUSE_MAINTAINER_MODE=1* - generate lex/yacc and errors files
 * *-DUSE_BACKTRACE=1* - add backtraces to native code asserts & exceptions
 * *-DUSE_FAILURE_TESTS=1* - adds javascript hook to crash the server for data integrity tests
 * *-DUSE_CATCH_TESTS=On (default is On so this is set unless you explicitly disable it)

If you have made changes to errors.dat, remember to use the -DUSE_MAINTAINER_MODE flag.

CFLAGS
------
Add backtraces to cluster requests so you can easily track their origin:

    -DDEBUG_CLUSTER_COMM

V8 Special flags:

    -DENABLE_GDB_JIT_INTERFACE

(enable (broken) GDB intergation of JIT)
At runtime arangod needs to be started with these options:

    --javascript.v8-options="--gdbjit_dump"
    --javascript.v8-options="--gdbjit_full"

Debugging the build process
---------------------------
If the compile goes wrong for no particular reason, appending 'verbose=' adds more output. For some reason V8 has VERBOSE=1 for the same effect.

Temporary files and temp directories
------------------------------------
Depending on the native way ArangoDB tries to locate the temporary directory.

* Linux/Mac: the environment variable `TMPDIR` is evaluated.
* Windows: the [W32 API function GetTempPath()](https://msdn.microsoft.com/en-us/library/windows/desktop/aa364992%28v=vs.85%29.aspx) is called
* all platforms: `--temp.path` overrules the above system provided settings.

Runtime
-------
 * start arangod with `--console` to get a debug console
 * Cheapen startup for valgrind: `--server.rest-server false --javascript.gc-frequency 1000000 --javascript.gc-interval 65536 --scheduler.threads=1 --javascript.v8-contexts=1`
 * to have backtraces output set this on the prompt: `ENABLE_NATIVE_BACKTRACES(true)`

Startup
-------
We now have a startup rc file: `~/.arangod.rc`. It's evaled as javascript.
A sample version to help working with the arangod rescue console may look like that:

    ENABLE_NATIVE_BACKTRACES(true);
    internal = require("internal");
    fs = require("fs");
    db = internal.db;
    time = internal.time;
    timed = function (cb) {
      var s  = time();
      cb();
      return time() - s;
    };
    print = internal.print;

HINT: You shouldn't lean on these variables in your foxx services.
______________________________________________________________________________________________________

JSLint
======
(we switched to eslint a while back - this is still named jslint for historical reasons)

checker Script
--------------
use

    ./utils/gitjslint.sh

to lint your modified files.

    ./utils/jslint.sh

to find out whether all of your files comply to jslint. This is required to make continuous integration work smoothly.

if you want to add new files / patterns to this make target, edit the respective shell scripts.

to be safe from committing non-linted stuff add **.git/hooks/pre-commit** with:

    ./utils/jslint.sh


Use jslint standalone for your js file
--------------------------------------
If you want to search errors in your js file, jslint is very handy - like a compiler is for C/C++.
You can invoke it like this:

    bin/arangosh --jslint js/client/modules/@arangodb/testing.js

_____________________________________________________________________________________________________

ArangoDB Unittesting Framework
==============================
Dependencies
------------
* *Ruby*, *rspec*, *httparty* to install the required dependencies run:
  `cd UnitTests/HttpInterface; bundler`
* catch (compile time, shipped in the 3rdParty directory)


Filename conventions
====================
Special patterns in the test filenames are used to select tests to be executed or skipped depending on parameters:

-server
-------
Make use of existing external server. (example scripts/unittest http_server --server tcp://127.0.0.1:8529/ )

-cluster
--------
These tests will only run if clustering is tested. (option 'cluster' needs to be true).

-noncluster
-----------
These tests will only run if no cluster is used. (option 'cluster' needs to be false)

-timecritical
-------------
These tests are critical to execution time - and thus may fail if arangod is to slow. This may happen i.e. if you run the tests in valgrind, so you want to avoid them since they will fail anyways. To skip them, set the option *skipTimeCritical* to *true*.

-disabled
---------
These tests are disabled. You may however want to run them by hand.

replication
-----------
(only applies to ruby tests)
These tests aren't run automatically since they require a manual set up environment.

-spec
-----
These tests are ran using the jasmine framework instead of jsunity.

-nightly
--------
These tests produce a certain thread on infrastructure or the test system, and therefore should only be executed once per day.

Test frameworks used
====================
There are several major places where unittests live: 
 - *UnitTests/HttpInterface*        - rspec tests
 - tests/*                          - catch unittests
 - *js/server/tests*                - runneable on the server
 - *js/common/tests*                - runneable on the server & via arangosh
 - *js/common/test-data*
 - *js/client/tests*                - runneable via arangosh
 - *js/apps/system/aardvark/test*


Debugging Tests (quick intro)
-----------------------------

runnuing single rspec test

   ./scripts/unittest http_server --test api-import-spec.rb

debugging rspec with gdb

    server> ./scripts/unittest http_server --test api-import-spec.rb --server tcp://127.0.0.1:7777
    - or -
    server> ARANGO_SERVER="127.0.0.1:6666" rspec -IUnitTests/HttpInterface --format d --color UnitTests/HttpInterface/api-import-spec.rb

    client> gdb --args ./build/bin/arangod --server.endpoint http+tcp://127.0.0.1:6666 --server.authentication false --log.level communication=trace ../arangodb-data-test-mmfiles

debugging a storage engine

    host> rm -fr ../arangodb-data-rocksdb/; gdb --args ./build/bin/arangod --console --server.storage-engine rocksdb --foxx.queues false --server.statistics false --server.endpoint http+tcp://0.0.0.0:7777 ../arangodb-data-rocksdb
    (gdb) catch throw
    (gdb) r
    arangod> require("jsunity").runTest("js/client/tests/shell/shell-client.js");



HttpInterface - RSpec Client Tests
----------------------------------
These tests work on the plain RESTfull interface of arangodb, and thus also test invalid HTTP-requests and so forth, plus check error handling in the server.

Running jsUnity tests
---------------------
Assume that you have a test file containing

```js
function exampleTestSuite () {
  return {
    testSizeOfTestCollection : function () {
    assertEqual(5, 5);
  };
}

jsUnity.run(aqlTestSuite);

return jsunity.done();
```

Then you can run the test suite using *jsunity.runTest*

```
arangosh> require("jsunity").runTest("test.js");
2012-01-28T19:10:23Z [10671] INFO Running aqlTestSuite
2012-01-28T19:10:23Z [10671] INFO 1 test found
2012-01-28T19:10:23Z [10671] INFO [PASSED] testSizeOfTestCollection
2012-01-28T19:10:23Z [10671] INFO 1 test passed
2012-01-28T19:10:23Z [10671] INFO 0 tests failed
2012-01-28T19:10:23Z [10671] INFO 1 millisecond elapsed
```

jsUnity on arangod
------------------
you can engage single tests when running arangod with console like this:

    require("jsunity").runTest("js/server/tests/aql-queries-simple.js");


jsUnity via arangosh
--------------------
arangosh is similar, however, you can only run tests which are intended to be ran via arangosh:

    require("jsunity").runTest("js/client/tests/shell/shell-client.js");

mocha tests
-----------
All tests with -spec in their names are using the [mochajs.org](https://mochajs.org) framework.

Javascript framework
--------------------
(used in our local Jenkins and TravisCI integration; required for running cluster tests)

Invoked like that:

    scripts/unittest all

calling it without parameters like this:

    scripts/unittest

will give you a extensive usage help which we won't duplicate here.

Choosing facility
_________________

The first parameter chooses the facility to execute.
Available choices include:
 - *all*:                (calls multiple) This target is utilized by most of the jenkins builds invoking unit tests.
 - *single_client*:      (see Running a single unittestsuite)
 - *single_server*:      (see Running a single unittestsuite)
 - many more -           call without arguments for more details.

Passing Options
---------------

Different facilities may take different options. The above mentioned usage output contains
the full detail.

A commandline for running a single test (-> with the facility 'single_server') using
valgrind could look like this. Options are passed as regular long values in the
syntax --option value --sub:option value. Using Valgrind could look like this:

    ./scripts/unittest single_server --test js/server/tests/aql/aql-escaping.js \
      --extraArgs:server.threads 1 \
      --extraArgs:scheduler.threads 1 \
      --extraArgs:javascript.gc-frequency 1000000 \
      --extraArgs:javascript.gc-interval 65536 \
      --javascript.v8-contexts 2 \
      --valgrind /usr/bin/valgrind \
      --valgrindargs:log-file /tmp/valgrindlog.%p

 - we specify the test to execute
 - we specify some arangod arguments via --extraArgs which increase the server performance
 - we specify to run using valgrind (this is supported by all facilities)
 - we specify some valgrind commandline arguments

Running a single unittestsuite
------------------------------
Testing a single test with the framework directly on a server:

    scripts/unittest single_server --test js/server/tests/aql/aql-escaping.js

Testing a single test with the framework via arangosh:

    scripts/unittest single_client --test js/server/tests/aql/aql-escaping.js

Testing a single rspec test:

    scripts/unittest http_server --test api-users-spec.rb

**scripts/unittest** is mostly only a wrapper; The backend functionality lives in:
**js/client/modules/@arangodb/testing.js**

Running foxx tests with a fake foxx Repo
----------------------------------------
Since downloading fox apps from github can be cumbersome with shaky DSL
and DOS'ed github, we can fake it like this:

    export FOXX_BASE_URL="http://germany/fakegit/"
    ./scripts/unittest single_server --test 'js/server/tests/shell/shell-foxx-manager-spec.js'

arangod Emergency console
-------------------------

    require("jsunity").runTest("js/server/tests/aql/aql-escaping.js");

arangosh client
---------------

    require("jsunity").runTest("js/server/tests/aql/aql-escaping.js");


arangod commandline arguments
-----------------------------

    bin/arangod /tmp/dataUT --javascript.unit-tests="js/server/tests/aql/aql-escaping.js" --no-server

    js/common/modules/loadtestrunner.js

__________________________________________________________________________________________________________

Linux Coredumps
===============
Generally coredumps have to be enabled using:

     ulimit -c unlimited

You should then see:

     ulimit -a
     core file size          (blocks, -c) unlimited

for each shell and its subsequent processes.

Hint: on Ubuntu the `apport` package may interfere with this; however you may use the `systemd-coredump` package
which automates much of the following:

So that the unit testing framework can autorun gdb it needs to reliably find the corefiles.
In Linux this is configured via the `/proc` filesystem, you can make this reboot permanent by
creating the file `/etc/sysctl.d/corepattern.conf` (or add the following lines to `/etc/sysctl.conf`)

    # We want core files to be located in a central location
    # and know the PID plus the process name for later use.
    kernel.core_uses_pid = 1
    kernel.core_pattern =  /var/tmp/core-%e-%p-%t

Note that the `proc` paths translate sub-directories to dots. The non permanent way of doing this in a running system is:

    echo 1 > /proc/sys/kernel/core_uses_pid
    echo '/var/tmp/core-%e-%p-%t' > /proc/sys/kernel/core_pattern

Solaris Coredumps
=================
Solaris configures the system corefile behaviour via the `coreadm` programm.
see https://docs.oracle.com/cd/E19455-01/805-7229/6j6q8svhr/ for more details.

Analyzing Coredumps on Linux
============================
We offer debug packages containing the debug symbols for your binaries. Please install them if you didn't compile yourselves.

Given you saw in the log of the arangod with the PID `25216` that it died, you should then find 
`/var/tmp/core-V8 WorkerThread-25216-1490887259` with this information. We may now start GDB and inspect whats going on:

    gdb /usr/sbin/arangod /var/tmp/*25216*

These commands give usefull information about the incident:

    backtrace full
    thread apply all bt

The first gives the full stacktrace including variables of the last active thread, the later one the stacktraces of all threads.

Windows debugging
=================
For the average \*nix user windows debugging has some awkward methods.

Coredump generation
-------------------
Coredumps can be created using the task manager; switch it to detail view, the context menu offers to *create dump file*; the generated file ends in a directory that explorer hides from you - AppData - you have to type that in the location bar.
This however only for running processes which is not as useful as having dumps of crashing processes. While its a common feature to turn on coredumps with the system facilities on \*nix systems, its not as easy in windows.
You need an external program [from the Sysinternals package: ProcDump](https://technet.microsoft.com/en-us/sysinternals/dd996900.aspx). First look up the PID of arangod, you can finde it in the brackets in the arangodb logfile. Then invoke *procdump* like this:

    procdump -accepteula -e -ma < PID of arangod >

It will keep on running and monitor arangod until eventually a crash happens. You will then get a core dump if an incident occurs or *Dump count not reached.* if nothing happened, *Dump count reached.* if a dump was written - the filename will be printed above.

Debugging symbols
-----------------
Releases are supported by a public symbol server so you will be able to debug cores.
Releases starting with **2.5.6, 2.6.3** onwards are supported; Note that you should run the latest version of a release series before reporting bugs.
Either [WinDbg](http://go.microsoft.com/fwlink/p/?linkid=84137) or Visual studio support setting the symbol path
via the environment variable or in the menu. Given we want to store the symbols on *e:\symbol_cach* we add the arangodb symbolserver like this:

    set _NT_SYMBOL_PATH=cache*e:\symbol_cache\cache;srv*e:\symbol_cache\arango*https://www.arangodb.com/repositories/symsrv/;SRV*e:\symbol_cache\ms*http://msdl.microsoft.com/download/symbols

You then will be able to see stack traces in the debugger.

You may also try to download the symbols manually using: 

    symchk.exe arangod.exe /s SRV*e:/symbol_cache/cache*https://www.arangodb.com/repositories/symsrv/


The symbolserver over at https://www.arangodb.com/repositories/symsrv/ is browseable; thus you can easily download the files you need by hand. It contains of a list of directories corosponding to the components of arangodb:

  - arango - the basic arangodb library needed by all components
  - arango_v8 - the basic V8 wrappers needed by all components
  - arangod - the server process 
  - the client utilities:
    - arangob
    - arangobench
    - arangoexport
    - arangoimp
    - arangorestore
    - arangosh
    - arangovpack

In these directories you will find subdirectories with the hash corosponding to the id of the binaries. Their date should corrospond to the release date of their respective arango release. 

This means i.e. for ArangoDB 3.1.11: 

 https://www.arangodb.com/repositories/symsrv/arangod.pdb/A8B899D2EDFC40E994C30C32FCE5FB346/arangod.pd_

This file is a microsoft cabinet file, which is a little bit compressed. You can dismantle it so the windows explorer offers you its proper handler by renaming it to .cab; click on the now named `arangod.cab`, copy the contained arangod.pdb into your symbol path.


Coredump analysis
-----------------
While Visual studio may cary a nice shiny gui, the concept of GUI fails miserably i.e. in testautomation. Getting an overview over all running threads is a tedious task with it. Here the commandline version of [WinDBG](http://www.windbg.org/) cdb comes to the aid. `testing.js` utilizes it to obtain automatical stack traces for crashes.
We run it like that:

    cdb -z <dump file> -c 'kp; ~*kb; dv; !analyze -v; q'

These commands for `-c` mean:
    kp           print curren threads backtrace with arguments
    ~*kb         print all threads stack traces
    dv           analyze local variables (if)
    !analyze -v  print verbose analysis
    q            quit the debugger

If you don't specify them via -c you can also use them in an interactive manner.

______________________________________________________________________________________________________

Documentation
=============
Using Docker container
----------------------
We provide the docker container `arangodb/documentation-builder` which brings all neccessary dependencies to build the documentation.

You can automagically build it using

    ./scripts/generateDocumentation.sh

which will start the docker container, compile ArangoDB, generate fresh example snippets, generate swagger, and all gitbook
produced output files.

You can also use `proselint` inside of that container to let it proof read your english ;-)


Installing on local system
--------------------------
Dependencies to build documentation:

- [swagger 2](http://swagger.io/) for the API-Documentation inside aardvark (no installation required)

- [Node.js](https://nodejs.org/)

    Make sure the option to *Add to PATH* is enabled.
    After installation, the following commands should be available everywhere:

    - `node`
    - `npm`

    If not, add the installation path to your environment variable PATH.
    Gitbook requires more recent node versions.

- [Gitbook](https://github.com/GitbookIO/gitbook)

    Can be installed with Node's package manager NPM:

        npm install gitbook-cli -g

- [ditaa (DIagrams Through Ascii Art)](http://ditaa.sourceforge.net/) to build the 
  ascii art diagrams (optional)

- Calibre2 (optional, only required if you want to build the e-book version)

  http://calibre-ebook.com/download

  Run the installer and follow the instructions.

Generate users documentation
============================
If you've edited examples, see below how to regenerate them with `./utils/generateExamples.sh`.
If you've edited REST (AKA HTTP) documentation, first invoke `./utils/generateSwagger.sh`.
Run `cd Documentation/Books && ./build.sh` to generate it.
The documentation will be generated in subfolders in `arangodb/Documentation/Books/books` -
use your favorite browser to read it.

You may encounter permission problems with gitbook and its npm invocations.
In that case, you need to run the command as root / Administrator.

If you see "device busy" errors on Windows, retry. Make sure you don't have
intermediate files open in the ppbooks / books subfolder (i.e. browser or editor).
It can also temporarily occur during phases of high HDD / SSD load.

The build-scripts contain several sanity checks, i.e. whether all examples are
used, and no dead references are there. (see building examples in that case below)

If the markdown files aren't converted to html, or `index.html` shows a single
chapter only (content of `README.md`), make sure
[Cygwin create native symlinks](https://docs.arangodb.com/cookbook/CompilingUnderWindows.html)
It does not, if `SUMMARY.md` in `Books/ppbooks/` looks like this:

    !<symlink>ÿþf o o

If sub-chapters do not show in the navigation, try another browser (Firefox).
Chrome's security policies are pretty strict about localhost and file://
protocol. You may access the docs through a local web server to lift the
restrictions. You can use pythons build in http server for this.

    ~/books$ python -m SimpleHTTPServer 8000

To only regereneate one file (faster) you may specify a filter:

    make build-book NAME=Manual FILTER=Manual/Aql/Invoke.md

(regular expressions allowed)

Using Gitbook
=============

The `arangodb/Documentation/Books/build.sh` script generates a website
version of the manual.

If you want to generate all media ala PDF, ePUB, run `arangodb/Documentation/books/build.sh  build-dist-books` (takes some time to complete).

If you want to generate only one of them, run below 
build commands in `arangodb/Documentation/Books/books/[Manual|HTTP|AQL]/`. Calibre's
`ebook-convert` will be used for the conversion.

Generate a PDF:

    gitbook pdf ./ppbooks/Manual ./target/path/filename.pdf

Generate an ePub:

    gitbook epub ./ppbooks/Manual ./target/path/filename.epub

Examples
========
Where to add new...
-------------------
 - Documentation/DocuBlocks/* - markdown comments with execution section
 - Documentation/Books/Manual/SUMMARY.md - index of all sub documentations

generate
--------
 - `./utils/generateExamples.sh --onlyThisOne geoIndexSelect` will only produce one example - *geoIndexSelect*
 - `./utils/generateExamples.sh --onlyThisOne 'MOD.*'` will only produce the examples matching that regex; Note that
   examples with enumerations in their name may base on others in their series - so you should generate the whole group.
 - running `onlyThisOne` in conjunction with a pre-started server cuts down the execution time even more.
   In addition to the `--onlyThisOne ...` specify i.e. `--server.endpoint tcp://127.0.0.1:8529` to utilize your already running arangod.
   Please note that examples may collide with existing collections like 'test' - you need to make sure your server is clean enough.
 - you can use generateExamples like that:
    `./utils/generateExamples.sh \
       --server.endpoint 'tcp://127.0.0.1:8529' \
       --withPython C:/tools/python2/python.exe \
       --onlyThisOne 'MOD.*'`
 - `./Documentation/Scripts/allExamples.sh` generates a file where you can inspect all examples for readability.
 - `./utils/generateSwagger.sh` - on top level to generate the documentation interactively with the server; you may use
    [the swagger editor](https://github.com/swagger-api/swagger-editor) to revalidate whether
    *js/apps/system/_admin/aardvark/APP/api-docs.json* is accurate.
 - `cd Documentation/Books; make` - to generate the HTML documentation


write markdown
--------------
*md* files are used for the structure. To join it with parts extracted from the program documentation
you need to place hooks:
  - `@startDocuBlock <tdocuBlockName>` is replaced by a Docublock extracted from source.
  - `@startDocuBlockInline <docuBlockName>` till `@endDocuBlock <docuBlockName>`
     is replaced in with its own evaluated content - so  *@EXAMPLE_ARANGOSH_[OUTPUT | RUN]* sections are executed
     the same way as inside of source code documentation.

Include ditaa diagrams
----------------------
We use the [beautifull ditaa (DIagrams Through Ascii Art)](http://ditaa.sourceforge.net/) to generate diagrams explaining flows etc.
in our documentation.

We have i.e. `Manual/Graphs/graph_user_in_group.ditaa` which is transpiled by ditaa into a png file, thus you simply include
a png file of the same name as image into the mardown: `![User in group example](graph_user_in_group.png)` to reference it.

Read / use the documentation
----------------------------
 - `file:///Documentation/Books/books/Manual/index.html` contains the generated manual
 - JS-Console - Tools/API - Interactive swagger documentation which you can play with.

arangod Example tool
====================
`./utils/generateExamples.sh` picks examples from the code documentation, executes them, and creates a transcript including their results.

Here is how its details work:
  - all *Documentation/DocuBlocks/*.md* and *Documentation/Books/*.md* are searched.
  - all lines inside of source code starting with '///' are matched, all lines in .md files.
  - an example start is marked with *@EXAMPLE_ARANGOSH_OUTPUT* or *@EXAMPLE_ARANGOSH_RUN*
  - the example is named by the string provided in brackets after the above key
  - the output is written to `Documentation/Examples/<name>.generated`
  - examples end with *@END_EXAMPLE_[OUTPUT|RUN]*
  - all code in between is executed as javascript in the **arangosh** while talking to a valid **arangod**.
  You may inspect the generated js code in `/tmp/arangosh.examples.js`

OUTPUT and RUN specifics
---------------------------
By default, Examples should be self contained and thus not depend on each other. They should clean up the collections they create.
Building will fail if resources aren't cleaned.
However, if you intend a set of OUTPUT and RUN to demonstrate interactively and share generated *ids*, you have to use an alphabetical
sortable naming scheme so they're executed in sequence. Using `<modulename>_<sequencenumber>[a|b|c|d]_thisDoesThat` seems to be a good scheme.

  - OUTPUT is intended to create samples that the users can cut'n'paste into their arangosh. Its used for javascript api documentation.
    * wrapped lines:
      Lines starting with a pipe (`/// |`) are joined together with the next following line.
      You have to use this if you want to execute loops, functions or commands which shouldn't be torn apart by the framework.
    * Lines starting with *var*:
      The command behaves similar to the arangosh: the server reply won't be printed.
      However, the variable will be in the scope of the other lines - else it won't.
    * Lines starting with a Tilde (`/// ~`):
      These lines can be used for setup/teardown purposes. They are completely invisible in the generated example transcript.
    * `~removeIgnoreCollection("test")` - the collection test may live across several tests.
    * `~addIgnoreCollection("test")` - the collection test will again be alarmed if left over.

  - it is executed line by line. If a line is intended to fail (aka throw an exception),
    you have to specify `// xpError(ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED)` so the exception will be caught;
    else the example is marked as broken.
    If you need to wrap that line, you may want to make the next line starting by a tilde to suppress an empty line:

        /// | someLongStatement()
        /// ~ // xpError(ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED)

 - RUN is intended to be pasted into a unix shell with *cURL* to demonstrate how the REST-HTTP-APIs work.
   The whole chunk of code is executed at once, and is expected **not to throw**.
   You should use **assert(condition)** to ensure the result is what you've expected.
   The *body* can be a string, or a javascript object which is then represented in JSON format.

    * Send the HTTP-request: `var response = logCurlRequest('POST', url, body);`
    * check its response:    `assert(response.code === 200);`
    * output a JSON server Reply: `logJsonResponse(response);` (will fail if its not a valid json)
    * output HTML to the user: `logHtmlResponse(response);` (i.e. redirects have HTML documents)
    * output the plain text to dump to the user: `logRawResponse(response);`
    * dump the reply to the errorlog for testing (will mark run as failed): `logErrorResponse(response);`


Swagger integration
===================
`./utils/generateSwagger.sh` scans the documentation, and generates swagger output.
It scans for all documentationblocks containing `@RESTHEADER`.
It is a prerequisite for integrating these blocks into the gitbook documentation.

Tokens may have curly brackets with comma separated fields. They may optionally be followed by subsequent text
lines with a long descriptions.

Sections group a set of tokens; they don't have parameters.

**Types**
Swagger has several native types referenced below:
*[integer|long|float|double|string|byte|binary|boolean|date|dateTime|password]* -
[see the swagger documentation](https://github.com/swagger-api/swagger-spec/blob/master/versions/2.0.md#data-types)

It consists of several sections which in term have sub-parameters:

**Supported Sections:**

RESTQUERYPARAMETERS
-------------------
Parameters to be appended to the URL in form of ?foo=bar
add *RESTQUERYPARAM*s below

RESTURLPARAMETERS
-----------------
Section of parameters placed in the URL in curly brackets, add *RESTURLPARAM*s below.

RESTDESCRIPTION
---------------
Long text describing this route.

RESTRETURNCODES
---------------
should consist of several *RESTRETURNCODE* tokens.

**Supported Tokens:**

RESTHEADER
----------
Up to 2 parameters.
* *[GET|POST|PUT|DELETE] <url>* url should start with a */*, it may contain parameters in curly brackets, that
  have to be documented in subsequent *RESTURLPARAM* tokens in the *RESTURLPARAMETERS* section.


RESTURLPARAM
------------
Consists of 3 values separated by ',':
Attributes:
  - *name*: name of the parameter
  - *type*:
  - *[required|optionas]* Optional is not supported anymore. Split the docublock into one with and one without.

Folowed by a long description.

RESTALLBODYPARAM
----------------
This API has a schemaless json body (in doubt just plain ascii)

RESTBODYPARAM
-------------
Attributes:
  - name - the name of the parameter
  - type - the swaggertype of the parameter
  - required/optional - whether the user can omit this parameter
  - subtype / format (can be empty)
    - subtype: if type is object or array, this references the enclosed variables.
               can be either a swaggertype, or a *RESTRUCT*
    - format: if type is a native swagger type, some support a format to specify them

RESTSTRUCT
----------
Groups a set of attributes under the `structure name` to an object that can be referenced
in other *RESTSTRUCT*s or *RESTBODYPARAM* attributes of type array or object

Attributes:
  - name - the name of the parameter
  - structure name - the **type** under which this structure can be reached (should be uniq!)
  - type - the swaggertype of the parameter (or another *RESTSTRUCT*...)
  - required/optional - whether the user can omit this parameter
  - subtype / format (can be empty)
    - subtype: if type is object or array, this references the enclosed variables.
               can be either a swaggertype, or a *RESTRUCT*
    - format: if type is a native swagger type, some support a format to specify them


--------------------------------------------------------------------------------
Local cluster startup
=====================

There are two scripts `scripts/startLocalCluster` and
`scripts/stopLocalCluster` which help you to quickly fire up a testing
cluster on your local machine. `scripts/startLocalCluster` takes 0, 2 or
three arguments. In the 0 argument version, it simply starts 2 DBservers
and one coordinator in the background, running on ports 8629, 8630 and
8530 respectively. The agency runs on port 4001. With 2 arguments the
first is the number of DBServers and the second is the number of 
coordinators.

If there is a third argument and it is "C", then the first coordinator
will be started with `--console`` in a separate window (using an
`xterm`). 

If there is a third argument and it is "D", then all servers are started
up in the GNU debugger in separate windows (using `xterm`s). In that
case one has to hit ENTER in the original terminal where the script runs
to continue, once all processes have been start up in the debugger.

ArangoDB on Mesos
=================

This will spawn a **temporary** local mesos cluster.

Requirements:

- Somewhat recent linux
- docker 1.10+
- curl
- sed (for file editing)
- jq (for json parsing)
- git
- at least 8GB RAM
- fully open firewall inside the docker network

To startup a local mesos cluster:

```
git clone https://github.com/m0ppers/mesos-cluster
cd mesos-cluster
mkdir /tmp/mesos-cluster
./start-cluster.sh /tmp/mesos-cluster/ --num-slaves=5 --rm --name mesos-cluster
```

Then save the following configuration to a local file and name it `arangodb3.json`:

```
{
  "id": "arangodb",
  "cpus": 0.25,
  "mem": 256.0,
  "ports": [0, 0, 0],
  "instances": 1,
  "args": [
    "framework",
    "--framework_name=arangodb",
    "--master=zk://172.17.0.2:2181/mesos",
    "--zk=zk://172.17.0.2:2181/arangodb",
    "--user=",
    "--principal=pri",
    "--role=arangodb",
    "--mode=cluster",
    "--async_replication=false",
    "--minimal_resources_agent=mem(*):512;cpus(*):0.25;disk(*):512",
    "--minimal_resources_dbserver=mem(*):1024;cpus(*):0.25;disk(*):1024",
    "--minimal_resources_secondary=mem(*):1024;cpus(*):0.25;disk(*):1024",
    "--minimal_resources_coordinator=mem(*):1024;cpus(*):0.25;disk(*):1024",
    "--nr_agents=3",
    "--nr_dbservers=2",
    "--nr_coordinators=2",
    "--failover_timeout=86400",
    "--arangodb_privileged_image=false",
    "--arangodb_force_pull_image=true",
    "--arangodb_image=arangodb/arangodb-mesos:3.0",
    "--secondaries_with_dbservers=false",
    "--coordinators_with_dbservers=false"
  ],
  "container": {
    "type": "DOCKER",
    "docker": {
      "image": "arangodb/arangodb-mesos-framework:3.0",
      "network": "HOST"
    }
  },
  "healthChecks": [
    {
      "protocol": "HTTP",
      "path": "/framework/v1/health.json",
      "gracePeriodSeconds": 3,
      "intervalSeconds": 10,
      "portIndex": 0,
      "timeoutSeconds": 10,
      "maxConsecutiveFailures": 0
    }
  ]
}
```

Adjust the lines `--master` and `--zk` to match the IP of your mesos-cluster:

```
MESOS_IP=`docker inspect mesos-cluster | \
  jq '.[0].NetworkSettings.Networks.bridge.IPAddress' | \
  sed 's;";;g'`
sed -i -e "s;172.17.0.2;${MESOS_IP};g" arangodb3.json
```

And deploy the modified file to your local mesos cluster:

```
MESOS_IP=`docker inspect mesos-cluster | \
  jq '.[0].NetworkSettings.Networks.bridge.IPAddress' | \
  sed 's;";;g'`
curl -X POST ${MESOS_IP}:8080/v2/apps \
      -d @arangodb3.json \
      -H "Content-Type: application/json" | \
  jq .
```

Point your webbrowser to the IP of your `echo "http://${MESOS_IP}:8080"`.

Wait until arangodb is healthy.

Then click on `arangodb`.

On the following screen click on the first port next to the IP Address of your cluster.

Deploying a locally changed version
-----------------------------------

Create local docker images using the following repositories:

 - https://github.com/arangodb/arangodb-docker
 - https://github.com/arangodb/arangodb-mesos-docker
 - https://github.com/arangodb/arangodb-mesos-framework

Then adjust the docker images in the config (`arangodb3.json`) and redeploy it using the curl command above.

--------------------------------------------------------------------------------
Front-End (WebUI)
=========

To see the changes of possible modifications in javascript files, templates
and scss files please use grunt to generate the bundled version. 

To install grunt (with all related dependencies), just go to the frontend app
folder (/js/apps/system/_admin/aardvark/APP) and run: 

`npm install`

On Mac OS you also have to install grunt-cli:

`(sudo) npm install -g grunt-cli`

Then you can choose between three choices:

1. Build all arangodb related files:

  * `grunt`

2. Build all arangodb related files, including libraries. This should always
be used when we offer a new major release of arangodb.

  * `grunt deploy`

3. Live build arangodb related files, when a file has been changed. This task
does not include the minifying process.

  * `grunt watch`

--------------------------------------------------------------------------------
NPM dependencies
=======

To add new NPM dependencies switch into the `js/node` folder and install them
with npm using the following options:

`npm install [<@scope>/]<name> --global-style --save --save-exact`

or simply

`npm install [<@scope>/]<name> --global-style -s -E`

The `save` and `save-exact` options are necessary to make sure the `package.json`
file is updated correctly.

The `global-style` option prevents newer versions of npm from unrolling nested
dependencies inside the `node_modules` folder. Omitting this option results in
exposing *all* dependencies of *all* modules to ArangoDB users.

Finally add the module's licensing information to `LICENSES-OTHER-COMPONENTS.md`.

When updating dependencies make sure that any mocked dependencies (like `glob`
for `mocha`) match the versions required by the updated module and delete any
duplicated nested dependencies if necessary (e.g. `mocha/node_modules/glob`)
to make sure the global (mocked) version is used instead.

