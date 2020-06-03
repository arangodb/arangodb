# ArangoDB Maintainers Manual

This file contains documentation about the build process and unittests.
Put short: if you want to hack parts of arangod this could be interesting
for you.

For the documentation about the documentation system and process see the
[docs repository](https://github.com/arangodb/docs#readme) instead.

Main sections:

- [Source Code](#source-code)
- [Building](#building)
- [Running](#running)
- [Debugging](#debugging)
  - [Linux Core Dumps](#linux-core-dumps)
  - [Windows Core Dumps](#windows-core-dumps)
- [Unittests](#unittests)

---

## Source Code

### Git

Setting up git for automatically merging certain automatically generated files
in the ArangoDB source tree:

    git config --global merge.ours.driver true

### Style Guide

We use `clang-format`.

### Unique Log Ids

We have unique log ids in order to allow for easy locating of code producing
errors.

    LOG_TOPIC("2dead", ....)

To ensure that the ids are unique we run the script `./utils/checkLogIds.py`
during CI runs. The script will fail with a non-zero status if id collisions are
found. You can use `openssl rand -hex 3 | sed 's/.//;s/\(.*\)/"\1"/'` or
anything that suits you to generate a **5 hex digit log** id.

### JSLint

We switched to eslint a while back, but it is still named jslint for historical
reasons.

#### Checker Script

Use:

    ./utils/gitjslint.sh

to lint your modified files.

    ./utils/jslint.sh

to find out whether all of your files comply to jslint.  This is required to
make continuous integration work smoothly.

If you want to add new files / patterns to this make target, edit the respective
shell scripts.

To be safe from committing non-linted stuff add **.git/hooks/pre-commit** with:

    ./utils/jslint.sh

#### Use jslint standalone for your js file

If you want to search errors in your js file, jslint is very handy - like a
compiler is for C/C++. You can invoke it like this:

    bin/arangosh --jslint js/client/modules/@arangodb/testing.js

---

## Building

### Building the binaries

ArangoDB uses a build system called [Oskar](https://github.com/arangodb/oskar).
Please refer to the documentation of Oskar for details.

For building the ArangoDB starter checkout the
[ArangoDB Starter](https://github.com/arangodb-helper/arangodb).

### Building the Web Interface

To build Web UI, also known as frontend or *Aardvark*, use CMake in the
build directory:

    cmake --build . --target frontend

For Oskar you may use the following:

    shellInAlpineContainer

    apk add --no-cache nodejs npm && cd /work/ArangoDB/build && cmake --build . --target frontend

To remove all available node modules and start a clean installation run:

    cmake --build . --target frontend_clean

The frontend can also be built using these commands:

    cd <SourceRoot>/js/apps/system/_admin/aardvark/APP/react
    npm install
    npm run build

For development purposes, go to `js/apps/system/_admin/aardvark/APP/react` and
run:

    npm start

This will deploy a development server (Port: 3000) and automatically start your
favorite browser and open the web UI.

All changes to any source will automatically re-build and reload your browser.
Enjoy :)

#### NPM Dependencies

To add new NPM dependencies switch into the `js/node` folder and install them
with npm using the following options:

`npm install [<@scope>/]<name> --global-style --save --save-exact`

or simply

`npm install [<@scope>/]<name> --global-style -s -E`

The `save` and `save-exact` options are necessary to make sure the
`package.json` file is updated correctly.

The `global-style` option prevents newer versions of npm from unrolling nested
dependencies inside the `node_modules` folder. Omitting this option results in
exposing *all* dependencies of *all* modules to ArangoDB users.

Finally add the module's licensing information to
`LICENSES-OTHER-COMPONENTS.md`.

When updating dependencies make sure that any mocked dependencies (like `glob`
for `mocha`) match the versions required by the updated module and delete any
duplicated nested dependencies if necessary (e.g. `mocha/node_modules/glob`) to
make sure the global (mocked) version is used instead.

---

## Running

### Temporary files and temp directories

Depending on the platform, ArangoDB tries to locate the temporary directory:

- Linux/Mac: the environment variable `TMPDIR` is evaluated.
- Windows: the [W32 API function GetTempPath()](https://msdn.microsoft.com/en-us/library/windows/desktop/aa364992%28v=vs.85%29.aspx) is called
- all platforms: `--temp.path` overrules the above system provided settings.

### Local Cluster Startup

The scripts `scripts/startLocalCluster` helps you to quickly fire up a testing
cluster on your local machine. `scripts/stopLocalCluster` stops it again.

`scripts/startLocalCluster [numDBServers numCoordinators [mode]]`

Without arguments it starts 2 DB-Servers and 1 Coordinator in the background,
running on ports 8629, 8630 and 8530 respectively. The Agency runs on port 4001.

Mode:
- `C`: Starts the first Coordinator with `--console` in a separate window
  (using an `xterm`).
- `D`: Starts all DB-Servers in the GNU debugger in separate windows
  (using `xterm`s). Hit *ENTER* in the original terminal where the script
  runs to continue once all processes have been started up in the debugger.

---

## Debugging

### Runtime

- start arangod with `--console` to get a debug console
- Cheapen startup for valgrind: `--server.rest-server false`
- to have backtraces output set this on the prompt: `ENABLE_NATIVE_BACKTRACES(true)`

### Startup

Arangod has a startup rc file: `~/.arangod.rc`. It's evaled as JavaScript.  A
sample version to help working with the arangod rescue console may look like
that:

    ENABLE_NATIVE_BACKTRACES(true);
    internal = require("internal");
    fs = require("fs");
    db = internal.db;
    time = internal.time;
    timed = function (cb) {
      var s = time();
      cb();
      return time() - s;
    };
    print = internal.print;

*Hint*: You shouldn't lean on these variables in your Foxx services.

### Debugging AQL execution blocks

To debug AQL execution blocks, two steps are required:

- turn on logging for queries using `--extraArgs:log.level queries=info`
- send queries enabling block debugging: `db._query('RETURN 1', {}, { profile: 4 })`

You now will get log entries with the contents being passed between the blocks.

### Crashes

The Linux builds of the arangod execuable contain a built-in crash handler
(introduced in v3.7.0).
The crash handler is supposed to log basic crash information to the ArangoDB logfile in
case the arangod process receives one of the signals SIGSEGV, SIGBUS, SIGILL, SIGFPE or
SIGABRT. SIGKILL signals, which the operating system can send to a process in case of OOM
(out of memory), are not interceptable and thus cannot be intercepted by the crash handler,

In case the crash handler receives one of the mentioned interceptable signals, it will
write basic crash information to the logfile and a backtrace of the call site.
The backtrace can be provided to the ArangoDB support for further inspection. Note that
backtaces are only usable if debug symbols for ArangoDB have been installed as well.

After logging the crash information, the crash handler will execute the default action for
the signal it has caught. If core dumps are enabled, the default action for these signals
is to generate a core file. If core dumps are not enabled, the crash handler will simply
terminate the program with a non-zero exit code.

The crash handler can be disabled at server start by setting the environment variable
`ARANGODB_OVERRIDE_CRASH_HANDLER` to `0` or `off`. 

An example log output from the crash handler looks like this:
```
2020-05-26T23:26:10Z [16657] FATAL [a7902] {crash} ArangoDB 3.7.1-devel enterprise [linux], thread 22 [Console] caught unexpected signal 11 (SIGSEGV) accessing address 0x0000000000000000: signal handler invoked
2020-05-26T23:26:10Z [16657] INFO [308c3] {crash} frame 1 [0x00007f9124e93ece]: _ZN12_GLOBAL__N_112crashHandlerEiP9siginfo_tPv (+0x000000000000002e)
2020-05-26T23:26:10Z [16657] INFO [308c3] {crash} frame 2 [0x00007f912687bfb2]: sigprocmask (+0x0000000000000021)
2020-05-26T23:26:10Z [16657] INFO [308c3] {crash} frame 3 [0x00007f9123e08024]: _ZN8arangodb3aql10Expression23executeSimpleExpressionEPKNS0_7AstNodeEPNS_11transaction7MethodsERbb (+0x00000000000001c4)
2020-05-26T23:26:10Z [16657] INFO [308c3] {crash} frame 4 [0x00007f9123e08314]: _ZN8arangodb3aql10Expression7executeEPNS0_17ExpressionContextERb (+0x0000000000000064)
2020-05-26T23:26:10Z [16657] INFO [308c3] {crash} frame 5 [0x00007f9123feaab2]: _ZN8arangodb3aql19CalculationExecutorILNS0_15CalculationTypeE0EE12doEvaluationERNS0_15InputAqlItemRowERNS0_16OutputAqlItemRowE (+0x0000000000000062)
2020-05-26T23:26:10Z [16657] INFO [308c3] {crash} frame 6 [0x00007f9123feae85]: _ZN8arangodb3aql19CalculationExecutorILNS0_15CalculationTypeE0EE11produceRowsERNS0_22AqlItemBlockInputRangeERNS0_16OutputAqlItemRowE (+0x00000000000000f5)
...
2020-05-26T23:26:10Z [16657] INFO [308c3] {crash} frame 31 [0x000018820ffc6d91]: *no symbol name available for this frame
2020-05-26T23:26:10Z [16657] INFO [ded81] {crash} available physical memory: 41721995264, rss usage: 294256640, vsz usage: 1217839104, threads: 46
Segmentation fault (core dumped)
```
The first line of the crash output will contain the cause of the crash (SIGSEGV in
this case). The following lines contain information about the stack frames. The 
hexadecimal value presented for each frame is the instruction pointer, and if debug 
symbols are installed, there will be name information about the called procedures (in
mangled format) plus the offsets into the procedures. If no debug symbols are
installed, symbol names and offsets cannot be shown for the stack frames.

### Core Dumps

A core dump consists of the recorded state of the working memory of a process at
a specific time. Such a file can be created on a program crash to analyze the
cause of the unexpected termination in a debugger.

#### Linux Core Dumps

##### Linux Core Dump Generation

Generally core dumps have to be enabled using:

     ulimit -c unlimited

You should then see:

     ulimit -a
     core file size          (blocks, -c) unlimited

for each shell and its subsequent processes.

*Hint*: on Ubuntu the `apport` package may interfere with this; however you may
use the `systemd-coredump` package which automates much of the following:

So that the unit testing framework can autorun gdb it needs to reliably find the
corefiles.  In Linux this is configured via the `/proc` filesystem, you can make
this reboot permanent by creating the file `/etc/sysctl.d/corepattern.conf` (or
add the following lines to `/etc/sysctl.conf`)

    # We want core files to be located in a central location
    # and know the PID plus the process name for later use.
    kernel.core_uses_pid = 1
    kernel.core_pattern =  /var/tmp/core-%e-%p-%t

to reload the above settings most systems support:

    sudo sysctl -p

Note that the `proc` paths translate sub-directories to dots.  The non permanent
way of doing this in a running system is:

    echo 1 > /proc/sys/kernel/core_uses_pid
    echo '/var/tmp/core-%e-%p-%t' > /proc/sys/kernel/core_pattern

(you may also inspect these files to validate the current settings)

More modern systems facilitate
[`systemd-coredump`](https://www.freedesktop.org/software/systemd/man/systemd-coredump.html)
(via a similar named package) to control core dumps.  On most systems it will
put compressed core dumps to `/var/lib/systemd/coredump`.

In order to use automatic core dump analysis with the unittests you need to
configure `/etc/systemd/coredump.conf` and set `Compress=no` - so instant
analysis may take place.

Please note that we can't support
[Ubuntu Apport](https://wiki.ubuntu.com/Apport).  Please use `apport-unpack` to
send us the bare core dumps.

In order to get core dumps from binaries changing their UID the system needs to
be told that its allowed to write cores from them. Default ArangoDB
installations will do exactly that, so the following is necessary to make the
system produce core dumps from production ArangoDB instances:

Edit `/etc/security/limits.conf` to contain:

```
arangodb        -       core            infinity
```

Edit the systemd unit file `/lib/systemd/system/arangodb3.service` (USE infinity!!!):

```
## setting for core files
# Any dir that is writable by the user running arangod
WorkingDirectory=/var/lib/arangodb3
# core limit - set this to infinity to enable cores
LimitCORE=0
```

Enable new systemd settings:

`systemctl daemon-reload &&  systemctl restart arangodb3.service`

Enable suid process dumping:

`echo 1 >/proc/sys/fs/suid_dumpable`

Make the above change permanent:

`echo "sys.fs.suid_dumpable = 1" >> /etc/sysctl.d/99-suid-coredump.conf`

**Please note that GDB 9 is required for ArangoDB 3.6 and later; GDB 8 is required for ArangoDB 3.4 and 3.5; GDB7 won't see threads**

You can also generate core dumps from running processes without killing them by
using gdb:

    # sleep 100000 &
    [2] 6942
    # gdb /bin/sleep 6942
    ...
    0x00007faaa7abd4e4 in __GI___nanosleep (requested_time=0x7ffd047c9940, remaining=0x0) at ../sysdeps/unix/sysv/linux/nanosleep.c:28
    gdb> gcore
    Saved corefile core.6942
    gdb> quit
    Detaching from program: /bin/sleep, process 6942
    # ls -l core*
    -rw-r--r--  1 me users  352664 Nov 27 10:48  core.6942

##### Installing GDB 8 on RedHat7 or Centos7

RedHat7 and Centos7 have a package called `devtoolset-7` which contains a
complete set of relative modern development tools. It can be installed by running

```
sudo yum install devtoolset-7
```

These will be installed under some path under `/opt`. To actually use these
tools, run

```
scl enable devtoolset-7 bash
```

to start a shell in which these are used. For example, you can pull a core dump
with `gcore` using this version, even for ArangoDB >= 3.4.

##### Analyzing Core Dumps on Linux

We offer debug packages containing the debug symbols for your binaries. Please
install them if you didn't compile yourselves.

Given you saw in the log of the arangod with the PID `25216` that it died, you
should then find `/var/tmp/core-V8 WorkerThread-25216-1490887259` with this
information. We may now start GDB and inspect whats going on:

    gdb /usr/sbin/arangod /var/tmp/*25216*

These commands give useful information about the incident:

    backtrace full
    thread apply all bt

The first gives the full stacktrace including variables of the last active
thread, the later one the stacktraces of all threads.

#### Windows Core Dumps

For the average \*nix user windows debugging has some awkward methods.

##### Windows Core Dump Generation

Core dumps can be created using the task manager; switch it to detail view, the
context menu offers to *create dump file*; the generated file ends in a
directory that explorer hides from you - AppData - you have to type that in the
location bar. This however only for running processes which is not as useful as
having dumps of crashing processes.

While it is a common feature to turn on core dumps with the system facilities on
\*nix systems, it is not as easy in Windows. You need an external program from
the *Sysinternals package*:
[ProcDump](https://technet.microsoft.com/en-us/sysinternals/dd996900.aspx).
First look up the PID of arangod, you can find it in the brackets in the
ArangoDB logfile. Then invoke *procdump* like this:

    procdump -accepteula -e -ma <PID-of-arangod>

It will keep on running and monitor arangod until eventually a crash happens.
You will then get a core dump if an incident occurs or *Dump count not reached.*
if nothing happened, *Dump count reached.* if a dump was written - the filename
will be printed above.

##### Windows Debugging Symbols

Releases are supported by a public symbol server so you will be able to debug
cores.  Please replace `XX` with the major and minor release number (e.g. `35`
for v3.5).  Note that you should run the latest version of a release series
before reporting bugs.  Either
[WinDbg](https://docs.microsoft.com/de-de/windows-hardware/drivers/debugger/debugger-download-tools)
or Visual Studio support setting the symbol path via the environment variable or
in the menu. Given we want to store the symbols on `E:\symbol_cache` we add the
ArangoDB symbolserver like this:

    set _NT_SYMBOL_PATH=SRV*e:\symbol_cache\arango*https://download.arangodb.com/symsrv_arangodbXX/;SRV*e:\symbol_cache\ms*http://msdl.microsoft.com/download/symbols

You then will be able to see stack traces in the debugger.

You may also try to download the symbols manually using:

    symchk.exe arangod.exe /s SRV*e:/symbol_cache/cache*https://download.arangodb.com/symsrv_arangodbXX/

The symbolserver over at https://download.arangodb.com/symsrv_arangodbXX/ is
browseable; thus you can easily download the files you need by hand.  It
contains of a list of directories corresponding to the components of ArangoDB:

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

In these directories you will find subdirectories with the hash corresponding to
the id of the binaries. Their date should correspond to the release date of
their respective arango release.

This means i.e. for ArangoDB 3.1.11:

 https://download.arangodb.com/symsrv_arangodb31/arangod.pdb/A8B899D2EDFC40E994C30C32FCE5FB346/arangod.pd_

This file is a microsoft cabinet file, which is a little bit compressed.  You
can extract it by invoking `cabextract` or dismantle it so the Windows Explorer
offers you its proper handler by renaming it to .cab; click on the now named
`arangod.cab`, copy the contained arangod.pdb into your symbol path.

##### Windows Core Dump Analysis

While Visual studio may carry a nice shiny GUI, the concept of GUI fails
miserably e.g. in test automation. Getting an overview over all running threads
is a tedious task with it. Here the commandline version of
[WinDbg](http://www.windbg.org/) cdb comes to the aid. `testing.js` utilizes it
to obtain automatical stack traces for crashes. We run it like that:

    cdb -z <dump file> -c 'kp; ~*kb; dv; !analyze -v; q'

These commands for `-c` mean:
    kp           print curren threads backtrace with arguments
    ~*kb         print all threads stack traces
    dv           analyze local variables (if)
    !analyze -v  print verbose analysis
    q            quit the debugger

If you don't specify them via `-c` you can also use them in an interactive
manner.

Alternatively you can also directly specify the symbol path via the `-y`
parameter (replace XX):

    cdb -z <dump file> -y 'SRV*e:\symbol_cache*https://download.arangodb.com/symsrv_arangodbXX;SRV*e:\symbol_cache\ms*http://msdl.microsoft'

and use the commands above to obtain stacktraces.

---

## Unittests

### Dependencies

- *Ruby*, *rspec*, *httparty*; to install the required dependencies run the
  following commands in the source root:
  ```
  gem install bundler
  cd tests/rb/HttpInterface
  bundler
  ```
- *Google Test* (compile time, shipped in the 3rdParty directory)

### Folder Locations

There are several major places where unittests live:

| Path                         | Description
|:-----------------------------|:-----------------------------
| `tests/js/server/`           | JavaScript tests, runnable on the server
| `tests/js/common/`           | JavaScript tests, runnable on the server & via arangosh
| `tests/js/common/test-data/` | Mock data used for the JavaScript tests
| `tests/js/client/`           | JavaScript tests, runnable via arangosh
| `tests/rb/`                  | rspec tests (Ruby)
| `tests/rb/HttpInterface/`    | rspec tests using the plain RESTful interface of ArangoDB. Include invalid HTTP requests and error handling checks for the server.
| `tests/` (remaining)         | Google Test unittests

### Filename conventions

Special patterns in the test filenames are used to select tests to be executed
or skipped depending on parameters:

| Substring       | Description
|:----------------|:----------------
| `-cluster`      | These tests will only run if clustering is tested (option 'cluster' needs to be true).
| `-noncluster`   | These tests will only run if no cluster is used (option 'cluster' needs to be false)
| `-timecritical` | These tests are critical to execution time - and thus may fail if arangod is to slow. This may happen i.e. if you run the tests in valgrind, so you want to avoid them since they will fail anyways. To skip them, set the option `skipTimeCritical` to *true*.
| `-disabled`     | These tests are disabled. You may however want to run them by hand.
| `-spec`         | These tests are run using the mocha framework instead of jsunity.
| `-nightly`      | These tests produce a certain thread on infrastructure or the test system, and therefore should only be executed once per day.
| `-grey`         | These tests are currently listed as "grey", which means that they are known to be unstable or broken. These tests will not be executed by the testing framework if the option `--skipGrey` is given. If `--onlyGrey` option is given then non-"grey" tests are skipped. See `tests/Greylist.txt` for up-to-date information about greylisted tests. Please help to keep this file up to date.

### JavaScript Framework

Since several testing technologies are utilized, and different ArangoDB startup
options may be required (even different compilation options may be required) the
framework is split into testsuites.

Get a list of the available testsuites and options by invoking:

    ./scripts/unittest

To locate the suite(s) associated with a specific test file use:

    ./scripts/unittest find --test tests/js/common/shell/shell-aqlfunctions.js

Run all suite(s) associated with a specific test file:

    ./scripts/unittest auto --test tests/js/common/shell/shell-aqlfunctions.js

Run all C++ based Google Test (gtest) tests using the `arangodbtests` binary:

    ./scripts/unittest gtest

Run specific gtest tests:

    ./scripts/unittest gtest --testCase "IResearchDocumentTest.*:*ReturnExecutor*"
    # equivalent to:
    ./build/bin/arangodbtests --gtest_filter="IResearchDocumentTest.*:*ReturnExecutor*"

Note that the `arangodbtests` executable is not compiled and shipped for
production releases (`-DUSE_GOOGLE_TESTS=off`).

Run all tests:

    scripts/unittest all

`scripts/unittest` is only a wrapper for the most part, the backend
functionality lives in `js/client/modules/@arangodb/` (`testing.js`,
`process-utils.js`, `test-utils.js`).  The actual testsuites are located in the
`testsuites` subfolder.

#### Passing Options

The first parameter chooses the facility to execute.  Available choices include:

 - **all**:                This target is utilized by most of the Jenkins builds invoking unit tests (calls multiple)
 - **single_client**:      (see [Running a single unittest suite](#running-a-single-unittest-suite))
 - **single_server**:      (see [Running a single unittest suite](#running-a-single-unittest-suite))

Different facilities may take different options. The above mentioned usage
output contains the full detail.

Instead of starting its own instance, `unittest` can also make use of a
previously started arangod instance. You can launch the instance as you want
including via a debugger or `rr` and prepare it for what you want to test with
it.  You then launch the test on it like this (assuming the default endpoint):

    ./scripts/unittest http_server --server tcp://127.0.0.1:8529/

A commandline for running a single test (-> with the facility 'single_server')
using valgrind could look like this. Options are passed as regular long values
in the syntax --option value --sub:option value. Using Valgrind could look like
this:

    ./scripts/unittest single_server --test tests/js/server/aql/aql-escaping.js \
      --extraArgs:server.threads 1 \
      --extraArgs:scheduler.threads 1 \
      --extraArgs:javascript.gc-frequency 1000000 \
      --extraArgs:javascript.gc-interval 65536 \
      --extraArgs:log.level debug \
      --extraArgs:log.force-direct true \
      --javascript.v8-contexts 2 \
      --valgrind /usr/bin/valgrind \
      --valgrindargs:log-file /tmp/valgrindlog.%p

- We specify the test to execute
- We specify some arangod arguments via --extraArgs which increase the server performance
- We specify to run using valgrind (this is supported by all facilities)
- We specify some valgrind commandline arguments
- We set the log level to debug
- We force the logging not to happen asynchronous
- Eventually you may still add temporary `console.log()` statements to tests you debug.

#### Running a Single Unittest Suite

Testing a single test with the framework directly on a server:

    scripts/unittest single_server --test tests/js/server/aql/aql-escaping.js

You can also only execute a filtered test case in a jsunity/mocha/gtest test
suite (in this case `testTokens`):

    scripts/unittest single_server --test tests/js/server/aql/aql-escaping.js --testCase testTokens

    scripts/unittest shell_client --test shell-util-spec.js --testCase zip

    scripts/unittest gtest --testCase "IResearchDocumentTest.*"

Testing a single test with the framework via arangosh:

    scripts/unittest single_client --test tests/js/client/shell/shell-transaction.js

Testing a single rspec test:

    scripts/unittest http_server --test api-users-spec.rb

Running a test against a server you started (instead of letting the script start its own server):

    scripts/unittest http_server --test api-batch-spec.rb --server tcp://127.0.0.1:8529 --serverRoot /tmp/123

#### Running Foxx Tests with a Fake Foxx Repo

Since downloading Foxx apps from GitHub can be cumbersome with shaky DSL and
DoS'ed GitHub, we can fake it like this:

    export FOXX_BASE_URL="http://germany/fakegit/"
    ./scripts/unittest single_server --test 'tests/js/server/shell/shell-foxx-manager-spec.js'

### Running jsUnity Tests

Assume that you have a test file containing:

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

Then you can run the test suite using `jsunity.runTest()`:

```
arangosh> require("jsunity").runTest("test.js");
2012-01-28T19:10:23Z [10671] INFO Running aqlTestSuite
2012-01-28T19:10:23Z [10671] INFO 1 test found
2012-01-28T19:10:23Z [10671] INFO [PASSED] testSizeOfTestCollection
2012-01-28T19:10:23Z [10671] INFO 1 test passed
2012-01-28T19:10:23Z [10671] INFO 0 tests failed
2012-01-28T19:10:23Z [10671] INFO 1 millisecond elapsed
```

#### Running jsUnity Tests with arangod

In (emergency) console mode (`arangod --console`):

    require("jsunity").runTest("tests/js/server/aql/aql-escaping.js");

Filtering for one test case (in this case `testTokens`) in console mode:

    require("jsunity").runTest("tests/js/server/aql/aql-escaping.js", false, "testTokens");

#### Running jsUnity Tests with arangosh client

Run tests this way:

    require("jsunity").runTest("tests/js/client/shell/shell-client.js");

You can only run tests which are intended to be ran via arangosh.

### Mocha Tests

All tests with `-spec` in their names are using the
[mochajs.org](https://mochajs.org) framework.  To run those tests, e.g. in the
arangosh, use this:

    require("@arangodb/mocha-runner").runTest('tests/js/client/endpoint-spec.js', true)

### Debugging Tests

This is quick introduction only.

Running a single rspec test:

    ./scripts/unittest http_server --test api-import-spec.rb

Debugging rspec with gdb:

    server> ./scripts/unittest http_server --test api-import-spec.rb --server tcp://127.0.0.1:7777
    - or -
    server> ARANGO_SERVER="127.0.0.1:6666" rspec -Itests/rb/HttpInterface --format d --color tests/rb/HttpInterface/api-import-spec.rb

    client> gdb --args ./build/bin/arangod --server.endpoint http+tcp://127.0.0.1:6666 --server.authentication false --log.level communication=trace ../arangodb-data-test-mmfiles

Debugging a storage engine:

    host> rm -fr ../arangodb-data-rocksdb/; gdb --args ./build/bin/arangod --console --server.storage-engine rocksdb --foxx.queues false --server.statistics false --server.endpoint http+tcp://0.0.0.0:7777 ../arangodb-data-rocksdb
    (gdb) catch throw
    (gdb) r
    arangod> require("jsunity").runTest("tests/js/client/shell/shell-client.js");

### Running tcpdump / windump for the SUT

Don't want to miss a beat of your test? If you want to invoke tcpdump with sudo,
make sure that your current shell has sudo enabled. Try like this:

    sudo /bin/true; ./scripts/unittest http_server \
      --sniff sudo --cleanup false

The pcap file will end up in your tests temporary directory.  You may need to
press an additional `ctrl+c` to force stop the sudo'ed tcpdump.

On Windows you can use TShark, you need a npcap enabled installation. List your
devices to sniff on using the -D option:

    c:/Program\ Files/wireshark/tshark.exe  -D
    1. \Device\NPF_{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} (Npcap Loopback Adapter)
    2. \Device\NPF_{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} (Ethernet)
    3. \\.\USBPcap1 (USBPcap1)

Choose the `Npcap Loopback Adapter` number - 1:

    ./scripts/unittest http_server \
      --sniff true \
      --cleanup false \
      --sniffDevice 1\
      --sniffProgram c:/Programm Files/wireshark/tshark.exe

You can later on use Wireshark to inpsect the capture files.


### Evaluating json test reports from previous testruns

All test results of testruns are dumped to a json file named
`UNITTEST_RESULT.json` which can be used for later analyzing of timings etc.

Currently available Analyzers are:

  - unitTestPrettyPrintResults - Prints a pretty summary and writes an ASCII representation into `out/testfailures.txt` (if any errors)
  - saveToJunitXML - saves jUnit compatible XML files
  - locateLongRunning - searches the 10 longest running tests from a testsuite
  - locateShortServerLife - whether the servers lifetime for the tests isn't at least 10 times as long as startup/shutdown
  - locateLongSetupTeardown - locate tests that may use a lot of time in setup/teardown
  - yaml - dumps the json file as a yaml file


    ./scripts/examine_results.js -- 'yaml,locateLongRunning' --readFile out/UNITTEST_RESULT.json
