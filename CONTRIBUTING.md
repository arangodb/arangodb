# Contributing

We welcome bug fixes and patches from 3rd party contributors. Please
see the [Contributor Agreement](https://www.arangodb.com/community#contribute)
for details.

Please follow these guidelines if you want to contribute to ArangoDB:

# Reporting Bugs

When reporting bugs, please use our issue tracker on GitHub. Please make sure
to include the version number of ArangoDB in your bug report, along with the
platform you are using (e.g. `Linux OpenSuSE x86_64`). Please also include the
ArangoDB startup mode (daemon, console, supervisor mode) plus any special
configuration. This will help us reproducing and finding bugs.

Please also take the time to check there are no similar/identical issues open
yet.

# Contributing features, documentation, tests

- Create a new branch in your fork, based on the **devel** branch

- Develop and test your modifications there

- Commit as you like, but preferably in logical chunks. Use meaningful commit
  messages and make sure you do not commit unnecessary files (e.g. object
  files). It is normally a good idea to reference the issue number from the
  commit message so the issues will get updated automatically with comments.

- If the modifications change any documented behavior or add new features,
  document the changes. It should be written in American English.
  The documentation can be found in [`docs-hugo` repository](https://github.com/arangodb/docs-hugo#readme).

- When done, run the complete test suite and make sure all tests pass.

- When finished, push the changes to your GitHub repository and send a pull
  request from your fork to the ArangoDB repository. Please make sure to select
  the appropriate branches there. This will most likely be **devel**.

- You must use the Apache License for your changes and have signed our
  [CLA](https://www.arangodb.com/documents/cla.pdf). We cannot accept pull requests
  from contributors that didn't sign the CLA.

- Please let us know if you plan to work on a ticket. This way we can make sure
  redundant work is avoided.

# Main sections

- [Source Code](#source-code)
- [Building](#building)
- [Running](#running)
- [Debugging](#debugging)
  - [Linux Core Dumps](#linux-core-dumps)
  - [Windows Core Dumps](#windows-core-dumps)
- [Unittests](#unittests)
  - [invoking driver tests](#driver-tests) via scripts/unittests
  - [capturing test HTTP communication](#running-tcpdump--windump-for-the-sut) but
    [forcing communication to use plain-text JSON](#forcing-downgrade-from-vpack-to-json)
  - [Evaluating previous testruns](#evaluating-json-test-reports-from-previous-testruns)
    sorting by setup time etc.
- [What to test where and how](tests/README.md)

## Source Code

### Git

Setting up git for automatically merging certain automatically generated files
in the ArangoDB source tree:

    git config --global merge.ours.driver true

### Style Guide

We use `clang-format` to enforce consistent code formatting. Check 
[STYLEGUIDE.md](STYLEGUIDE.md) for a comprehensive description of ArangoDB's 
Coding Guidelines.

### Compiler support policy

We support only officially released compiler versions which major version is
at least 9 month old.

### Unique Log Ids

We have unique log ids in order to allow for easy locating of code producing
errors.

    LOG_TOPIC("2dead", ....)

To ensure that the ids are unique we run the script `./utils/checkLogIds.py`
during CI runs. The script will fail with a non-zero status if id collisions are
found. You can use `openssl rand -hex 3 | sed 's/.//;s/\(.*\)/"\1"/'` or
anything that suits you to generate a **5 hex digit log** id.

### ESLint

Use:

    ./utils/giteslint.sh

to lint your modified files.

    ./utils/eslint.sh

to find out whether all of your files comply to eslint. This is required to
make continuous integration work smoothly.

If you want to add new files / patterns to this make target, edit the respective
shell scripts.

To be safe from committing non-linted stuff add **.git/hooks/pre-commit** with:

    ./utils/eslint.sh

### Adding startup options

Startup option example with explanations:

```cpp
options->
    addOption("--section.option-name",
              "A brief description of the startup option, maybe over multiple "
              "lines, with an upper-case first letter and ending with a .",
              // for numeric options, you may need to indicate the unit like "timeout (in seconds)"
                new XyzParameter(&_xyzVariable),
                arangodb::options::makeFlags(
                    arangodb::options::Flags::DefaultNoComponents,
                    arangodb::options::Flags::OnCoordinator, // in a cluster, it only has an effect on Coordinators
                    arangodb::options::Flags::OnSingle,      // supported in single server mode, too
                    arangodb::options::Flags::Enterprise,    // only available in the Enterprise Edition
                    arangodb::options::Flags::Uncommon))     // don't show with --help but only --help-all or --help-<section>
    .setIntroducedIn(30906) // format XYYZZ, X = major, YY = minor, ZZ = bugfix version
    .setIntroducedIn(31002) // list all versions the feature is added in but exclude versions that are implied, e.g. 31100
    .setLongDescription(R"(You can optionally add details here. They are only
shown in the online documentation (and --dump-options). 

The _text_ is interpreted as **Markdown**, allowing formatting like
`inline code`, fenced code blocks, and even tables.)");
```

See [`lib/ProgramOptions/Option.h`](lib/ProgramOptions/Option.h) for details.

For a feature that is added to v3.9.6, v3.10.2, and devel, you only need to set
`.setIntroducedIn()` for 3.9 and 3.10 but not 3.11 (in devel) because all later
versions (after v3.10.2) can reasonably be expected to include the feature, too.
Leaving v3.10.2 out would be unclear, however, because users may assume to find
the feature in v3.10.0 and v3.10.1 if it was added to v3.9.6.

In the 3.9 branch, you should only set the versions up to 3.9 but not 3.10,
pretending no later version exists to avoid additional maintenance burdens.

In 3.9:

```cpp
    .setIntroducedIn(30906)
```

In 3.10 and later:

```cpp
    .setIntroducedIn(30906)
    .setIntroducedIn(31002)
```

These version remarks can be removed again when all of the listed versions for
an option are obsolete (and removed from the online documentation).

Note that `.setDeprecatedIn()` should not be removed until the startup option is
obsoleted or fully removed.

### Adding metrics

As of 3.8 we have enforced documentation for metrics. This works as
follows. Every metric which is generated has a name. The metric must be
declared by using one of the macros

```
  DECLARE_COUNTER(name, helpstring);
  DECLARE_GAUGE(name, type, helpstring);
  DECLARE_HISTOGRAM(name, scaletype, helpstring);
```

in some `.cpp` file (please put only one on a line). Then, when the
metric is actually requested in the source code, it gets a template
added to the metrics feature with the `add` method and its name.
Labels can be added with the `withLabel` method. In this way, the
compiler ensures that the metric declaration is actually there if
the metric is used.

Then there is a helper script `utils/generateAllMetricsDocumentation.py`
which needs `python3` with the `yaml` module. It will check and do the
following things:

- Every declared metric in some `.cpp` file in the source has a
  corresponding documentation snippet in `Documentation/Metrics`
  under the name of the metric with `.yaml` appended
- Each such file is a YAML file of a certain format (see template
  under `Documentation/Metrics/template.yaml`)
- Many of the components are required, so please provide adequate
  information about your metric
- Make sure to set `introducedIn` to the version the metric is added to the
  current branch's version (e.g. `"3.9.6"` in the 3.9 branch, `"3.10.2"` in
  the 3.10 and devel branches)
- The script can also assemble all these YAML documentation snippets
  into a single file under `Documentation/Metrics/allMetrics.yaml`,
  the format is again a structured YAML file which can easily be
  processed by the documentation tools, this is only needed when
  we update the documentation web pages.

Please, if you have added or modified a metric, make sure to declare
the metric as shown above and add a documentation YAML snippet in the
correct format. Afterwards, run

    utils/generateAllMetricsDocumentation.py

but do not include `Documentation/Metrics/allMetrics.yaml` in your PR
(as was a previous policy). The file is only generated if you add
`-d` as command line option to the script.

---

## Building

Note: Make sure that your source path does not contain spaces otherwise the build process will likely fail.

### Building the binaries

ArangoDB uses a build system called [Oskar](https://github.com/arangodb/oskar).
Please refer to the documentation of Oskar for details.

Optimizations and limit of architecture (list of possible CPU instructions) are set using this `cmake` option
in addition to the other options:

```
-DTARGET_ARCHITECTURE
```

Oskar uses predefined architecture which is defined in `./VERSIONS` file or `sandybridge` if it's not defined.

Note: if you use more modern architecture for optimizations or any additional implementation with extended
set of CPU instructions please notice that result could be different to the default one.

For building the ArangoDB starter checkout the
[ArangoDB Starter](https://github.com/arangodb-helper/arangodb).

### Building the Web Interface

To build web interface, also known as the Web UI, frontend, or _Aardvark_, [Node.js](https://nodejs.org/)
and [Yarn](https://yarnpkg.com/) need to be installed. You can then use CMake in the
build directory:

    cmake --build . --target frontend

For Oskar you may use the following:

    shellInAlpineContainer

    apk add --no-cache nodejs yarn && cd /work/ArangoDB/build && cmake --build . --target frontend

To remove all available node modules and start a clean installation run:

    cmake --build . --target frontend_clean

The frontend can also be built using these commands:

    cd <SourceRoot>/js/apps/system/_admin/aardvark/APP/react
    yarn install
    yarn run build

For development purposes, go to `js/apps/system/_admin/aardvark/APP/react` and
run:

    yarn start

This will deploy a development server (Port: 3000) and automatically start your
favorite browser and open the web interface.

All changes to any source will automatically re-build and reload your browser.
Enjoy :)

#### Cross Origin Policy (CORS) ERROR

Our front-end development server currently runs on port:`3000`, while the backend
runs on port:`8529` respectively. This implies that when the front-end sends a
request to the backend would result in Cross-Origin-Policy security checks which
recently got enforced by some browsers for security reasons. Until recently, we
never had reports of CORS errors when running both the backend and front-end dev
servers independently, however, we recently confirmed that this error occurs in
(Chrome version 98.0.4758.102 and Firefox version 96.0.1).

In case you run into CORS errors while running the development server, here is a quick fix:

Simply stop your already running backend process with the command below:

```bash
cmd + c
```

Then restart `arangodb` server with the command below:

```bash
./build/bin/arangod ../Arango --http.trusted-origin '*'
```

Note:

a: `./build/bin/arangod`: represents the location of `arangodb` binaries in your machine.

b: `../Arango`: is the database directory where the data will be stored.

c: `--http.trusted-origin '*'` the prefix that allows cross-origin requests.

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
exposing _all_ dependencies of _all_ modules to ArangoDB users.

Finally add the module's licensing information to
`LICENSES-OTHER-COMPONENTS.md`.

If you need to make adjustments/modifications to dependencies or replace
transitive dependencies with mocks, make sure to run `npx patch-package $dependencyName`
in the `js/node` folder and commit the resulting patch file in `js/node/patches`.
This will ensure the changes are persisted if the dependency is overwritten by `npm`
in the future.

For example to commit a patch for the transitive dependency `is-wsl` of the dependency
`node-netstat`, make your changes in `js/node/node_modules/node-netstat/node_modules/is-wsl`
and then run `npx patch-package node-netstat/is-wsl` in `js/node` and commit the resulting
patch file in `js/node/patches`.

#### Build the HTTP API documentation for Swagger-UI

The REST HTTP API of the ArangoDB server is described using the OpenAPI
specification (formerly Swagger). The source code is in documentation repository
at <https://github.com/arangodb/docs-hugo>.

To build the `api-docs.json` file for viewing the API documentation in the
Swagger-UI of the web interface (**SUPPORT** section, **Rest API** tab), run
the following commands in a terminal:

1. Get a working copy of the documentation content with Git:

   `git clone https://github.com/arangodb/docs-hugo`

2. Enter the `docs-hugo` folder:

   `cd docs-hugo`

3. Optional: Switch to a tag, branch, or commit if you want to build the
   API documentation for a specific version of the docs:
   
   `git checkout <reference>`

4. Enter the folder of the Docker toolchain, `amd64` on the x86-64 architecture
   and `arm64` on ARM CPUs:

   ```shell
   cd toolchain/docker/amd64 # x86-64
   cd toolchain/docker/arm64 # ARM 64-bit
   ```

5. Set the environment variable `ENV` to any value other than `local` to make
   the documentation tooling not start a live server in watch mode but rather
   create and static build and exit:

   ```shell
   export ENV=static  # Bash
   set -xg ENV static # Fish
   $Env:ENV='static'  # PowerShell
   ```

6. Run Docker Compose using the plain build configuration for the documentation:

   `docker compose -f docker-compose.plain-build.yml up --abort-on-container-exit`

7. When the docs building finishes successfully, you can find the `api-docs.json`
   files in `site/data/<version>/`.

8. Copy the respective `api-docs.json` file into the ArangoDB working copy or
   installation folder under `js/apps/system/_admin/aardvark/APP/api-docs.json`
   and refresh the web interface.

---

## Running

### Temporary files and temp directories

Depending on the platform, ArangoDB tries to locate the temporary directory:

- Linux/Mac: the environment variable `TMPDIR` is evaluated.
- Windows: the [W32 API function GetTempPath()](https://msdn.microsoft.com/en-us/library/windows/desktop/aa364992%28v=vs.85%29.aspx) is called
- all platforms: `--temp.path` overrules the above system provided settings.

### Local Cluster Startup

The scripts `scripts/startLocalCluster` helps you to quickly fire up a testing
cluster on your local machine. `scripts/shutdownLocalCluster` stops it again.

`scripts/startLocalCluster [numDBServers numCoordinators [mode]]`

Without arguments it starts 2 DB-Servers and 1 Coordinator in the background,
running on ports 8629, 8630 and 8530 respectively. The Agency runs on port 4001.

Mode:

- `C`: Starts the first Coordinator with `--console` in a separate window
  (using an `xterm`).
- `D`: Starts all DB-Servers in the GNU debugger in separate windows
  (using `xterm`s). Hit _ENTER_ in the original terminal where the script
  runs to continue once all processes have been started up in the debugger.

---

## Debugging

### Runtime

- start arangod with `--console` to get a debug console
- Cheapen startup for valgrind: `--server.rest-server false`
- to have backtraces output set this on the prompt: `ENABLE_NATIVE_BACKTRACES(true)`

### Startup

Arangod has a startup rc file: `~/.arangod.rc`. It's evaled as JavaScript. A
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

_Hint_: You shouldn't lean on these variables in your Foxx services.

### Debugging AQL execution blocks

To debug AQL execution blocks, two steps are required:

- turn on logging for queries using `--extraArgs:log.level queries=info`
- divert this facilities logoutput into individual files: `--extraArgs --log.output queries file://@ARANGODB_SERVER_DIR@/arangod_queries.log`
- send queries enabling block debugging: `db._query('RETURN 1', {}, { profile: 4 })`

You now will get log entries with the contents being passed between the blocks.

### Crashes

The Linux builds of the arangod executable contain a built-in crash handler
(introduced in v3.7.0).
The crash handler is supposed to log basic crash information to the ArangoDB logfile in
case the arangod process receives one of the signals SIGSEGV, SIGBUS, SIGILL, SIGFPE or
SIGABRT. SIGKILL signals, which the operating system can send to a process in case of OOM
(out of memory), are not interceptable and thus cannot be intercepted by the crash handler,

In case the crash handler receives one of the mentioned interceptable signals, it will
write basic crash information to the logfile and a backtrace of the call site.
The backtrace can be provided to the ArangoDB support for further inspection. Note that
backtraces are only usable if debug symbols for ArangoDB have been installed as well.

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

### Memory profiling

Starting in 3.6 we have support for heap profiling when using jemalloc on Linux.
Here is how it is used:

Use this `cmake` option in addition to the normal options:

```
-DUSE_JEMALLOC_PROF
```

when building. Make sure you are using the builtin `jemalloc`. You need
debugging symbols to analyze the heap dumps later on, so compile with
`-DCMAKE_BUILD_TYPE=Debug` or `RelWithDebInfo`. `Debug` is probably
less confusing in the end.

Then set the environment variable for each instance you want to profile:

```
export MALLOC_CONF="prof:true"
```

When this is done, `jemalloc` internally does some profiling. You can
then use this endpoint to get a heap dump:

```
curl "http://localhost:8529/_admin/status?memory=true" > heap.dump
```

You can analyze such a heap dump with the `jeprof` tool, either by
inspecting a single dump like this:

```
jeprof build/bin/arangod heap.dump
```

or compare two dumps which were done on the same process to analyze
the difference:

```
jeprof build/bin/arangod --base=heap.dump heap.dump2
```

So far, we have mostly used the `web` command to open a picture in a
browser. It produces an `svg` file.

The tool isn't perfect, but can often point towards the place which
produces for example a memory leak or a lot of memory usage.

This is known to work under Linux and the `jeprof` tool comes with the
`libjemalloc-dev` package on Ubuntu.

Using it with the integration tests is possible; snapshots will be taken before
the first and after each subsequent testcase executed.

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

_Hint_: on Ubuntu the `apport` package may interfere with this; however you may
use the `systemd-coredump` package which automates much of the following:

So that the unit testing framework can autorun gdb it needs to reliably find the
corefiles. In Linux this is configured via the `/proc` filesystem, you can make
this reboot permanent by creating the file `/etc/sysctl.d/corepattern.conf` (or
add the following lines to `/etc/sysctl.conf`)

    # We want core files to be located in a central location
    # and know the PID plus the process name for later use.
    kernel.core_uses_pid = 1
    kernel.core_pattern =  /var/tmp/core-%e-%p-%t

to reload the above settings most systems support:

    sudo sysctl -p

Note that the `proc` paths translate sub-directories to dots. The non permanent
way of doing this in a running system is:

    echo 1 > /proc/sys/kernel/core_uses_pid
    echo '/var/tmp/core-%e-%p-%t' > /proc/sys/kernel/core_pattern

(you may also inspect these files to validate the current settings)

More modern systems facilitate
[`systemd-coredump`](https://www.freedesktop.org/software/systemd/man/systemd-coredump.html)
(via a similar named package) to control core dumps. On most systems it will
put compressed core dumps to `/var/lib/systemd/coredump`.

In order to use automatic core dump analysis with the unittests you need to
configure `/etc/systemd/coredump.conf` and set `Compress=no` - so instant
analysis may take place.

Please note that we can't support
[Ubuntu Apport](https://wiki.ubuntu.com/Apport). Please use `apport-unpack` to
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

`systemctl daemon-reload && systemctl restart arangodb3.service`

Enable suid process dumping:

`echo 1 >/proc/sys/fs/suid_dumpable`

Make the above change permanent:

`echo "sys.fs.suid_dumpable = 1" >> /etc/sysctl.d/99-suid-coredump.conf`

**Please note that at least GDB 9 is required.**

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
context menu offers to _create dump file_; the generated file ends in a
directory that explorer hides from you - AppData - you have to type that in the
location bar. This however only for running processes which is not as useful as
having dumps of crashing processes.

While it is a common feature to turn on core dumps with the system facilities on
\*nix systems, it is not as easy in Windows. You need an external program from
the _Sysinternals package_:
[ProcDump](https://technet.microsoft.com/en-us/sysinternals/dd996900.aspx).
First look up the PID of arangod, you can find it in the brackets in the
ArangoDB logfile. Then invoke _procdump_ like this:

    procdump -accepteula -e -ma <PID-of-arangod>

It will keep on running and monitor arangod until eventually a crash happens.
You will then get a core dump if an incident occurs or _Dump count not reached._
if nothing happened, _Dump count reached._ if a dump was written - the filename
will be printed above.

##### Windows Debugging Symbols

Releases are supported by a public symbol server so you will be able to debug
cores. Please replace `XX` with the major and minor release number (e.g. `38`
for v3.8). Note that you should run the latest version of a release series
before reporting bugs. Either
[WinDbg](https://docs.microsoft.com/de-de/windows-hardware/drivers/debugger/debugger-download-tools)
or Visual Studio support setting the symbol path via the environment variable or
in the menu. Given we want to store the symbols on `E:\symbol_cache` we add the
ArangoDB symbolserver like this:

    set _NT_SYMBOL_PATH=SRV*e:\symbol_cache\arango*https://download.arangodb.com/symsrv_arangodbXX/;SRV*e:\symbol_cache\ms*http://msdl.microsoft.com/download/symbols

You then will be able to see stack traces in the debugger.

You may also try to download the symbols manually using:

    symchk.exe arangod.exe /s SRV*e:/symbol_cache/cache*https://download.arangodb.com/symsrv_arangodbXX/

The symbolserver over at https://download.arangodb.com/symsrv_arangodbXX/ is
browseable; thus you can easily download the files you need by hand. It
contains of a list of directories corresponding to the components of ArangoDB, e.g.:

- lib - the basic arangodb libraries needed by all components
- lib/V8 - the basic V8 wrappers needed by all components
- arangod - the server process
- the client utilities:
  - arangobackup
  - arangobench
  - arangodump
  - arangoexport
  - arangoimport
  - arangorestore
  - arangosh
  - arangovpack

In these directories you will find subdirectories with the hash corresponding to
the id of the binaries. Their date should correspond to the release date of
their respective arango release.

This means i.e. for ArangoDB 3.1.11:

https://download.arangodb.com/symsrv_arangodb31/arangod.pdb/A8B899D2EDFC40E994C30C32FCE5FB346/arangod.pd_

This file is a microsoft cabinet file, which is a little bit compressed. You
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
kp print curren threads backtrace with arguments
~\*kb print all threads stack traces
dv analyze local variables (if)
!analyze -v print verbose analysis
q quit the debugger

If you don't specify them via `-c` you can also use them in an interactive
manner.

Alternatively you can also directly specify the symbol path via the `-y`
parameter (replace XX):

    cdb -z <dump file> -y 'SRV*e:\symbol_cache*https://download.arangodb.com/symsrv_arangodbXX;SRV*e:\symbol_cache\ms*http://msdl.microsoft'

and use the commands above to obtain stacktraces.

---

## Unittests

### Dependencies

- _Google Test_ (compile time, shipped in the 3rdParty directory)

### Folder Locations

There are several major places where unittests live:

| Path / File                                                  | Description                                                                                                                        |
| :----------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------- |
| `tests/js/server/`                                           | JavaScript tests, runnable on the server                                                                                           |
| `tests/js/common/`                                           | JavaScript tests, runnable on the server & via arangosh                                                                            |
| `tests/js/common/test-data/`                                 | Mock data used for the JavaScript tests                                                                                            |
| `tests/js/client/`                                           | JavaScript tests, runnable via arangosh                                                                                            |
| `tests/` (remaining)                                         | Google Test unittests                                                                                                              |
| implementation specific files                                |
| `scripts/unittest`                                           | Entry wrapper script for `UnitTests/unittest.js`                                                                                   |
| `js/client/modules/@arangodb/testutils/testing.js`           | invoked via `unittest.js` handles module structure for `testsuites`.                                                               |
| `js/client/modules/@arangodb/testutils/test-utils.js`        | infrastructure for tests like filtering, bucketing, iterating                                                                      |
| `js/client/modules/@arangodb/testutils/process-utils.js`     | manage arango instances, start/stop/monitor SUT-processes                                                                          |
| `js/client/modules/@arangodb/testutils/result-processing.js` | work with the result structures to produce reports, hit lists etc.                                                                 |
| `js/client/modules/@arangodb/testutils/crash-utils.js`       | if something goes wrong, this contains the crash analysis tools                                                                   |
| `js/client/modules/@arangodb/testutils/clusterstats.js`      | can be launched separately to monitor the cluster instances and their resource usage                                               |
| `js/client/modules/@arangodb/testsuites/`                    | modules with testframework that control one set of tests each                                                                      |
| `js/common/modules[/jsunity]/jsunity.js`                     | jsunity testing framework; invoked via jsunity.js next to the module                                                               |
| `js/common/modules/@arangodb/mocha-runner.js`                | wrapper for running mocha tests in arangodb                                                                                        |

### Filename conventions

Special patterns in the test filenames are used to select tests to be executed
or skipped depending on parameters:

| Substring       | Description                                                                                                                                                                                                                                                                                                                                                                                   |
| :-------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `-cluster`      | These tests will only run if clustering is tested (option 'cluster' needs to be true).                                                                                                                                                                                                                                                                                                        |
| `-noncluster`   | These tests will only run if no cluster is used (option 'cluster' needs to be false)                                                                                                                                                                                                                                                                                                          |
| `-noasan`        | These tests will not be ran if *san instrumented binaries are used                                                                                                                                                                                                                                                                                                                           |
|  `-noinstr`      | These tests will not be ran if instrumented binaries are used, be it *san or gcov                                                                                                                                                                                                                                                                                                             | 
|  `-nocov`        | These tests will not be ran if gcov instrumented binaries are used.                                                                                                                                                                                                                                                                                                              | 
| `-timecritical` | These tests are critical to execution time - and thus may fail if arangod is to slow. This may happen i.e. if you run the tests in valgrind, so you want to avoid them since they will fail anyways. To skip them, set the option `skipTimeCritical` to _true_.                                                                                                                               |
| `-spec`         | These tests are run using the mocha framework instead of jsunity.                                                                                                                                                                                                                                                                                                                             |
| `-nightly`      | These tests produce a certain thread on infrastructure or the test system, and therefore should only be executed once per day.                                                                                                                                                                                                                                                                |
| `-grey`         | These tests are currently listed as "grey", which means that they are known to be unstable or broken. These tests will not be executed by the testing framework if the option `--skipGrey` is given. If `--onlyGrey` option is given then non-"grey" tests are skipped. See `tests/Greylist.txt` for up-to-date information about greylisted tests. Please help to keep this file up to date. |

### JavaScript Framework

Since several testing technologies are utilized, and different ArangoDB startup
options may be required (even different compilation options may be required) the
framework is split into testsuites.

Get a list of the available testsuites and options by invoking:

    ./scripts/unittest

To locate the suite(s) associated with a specific test file use:

    ./scripts/unittest find --test tests/js/common/shell/shell-aqlfunctions.js

Run all suite(s) associated with a specific test file in single server mode:

    ./scripts/unittest auto --test tests/js/common/shell/shell-aqlfunctions.js

Run all suite(s) associated with a specific test file in cluster mode:

    ./scripts/unittest auto --cluster true --test tests/js/common/shell/shell-aqlfunctions.js

Run all C++ based Google Test (gtest) tests using the `arangodbtests` binary:

    ./scripts/unittest gtest

Run specific gtest tests:

    ./scripts/unittest gtest --testCase "IResearchDocumentTest.*:*ReturnExecutor*"
    # equivalent to:
    ./build/bin/arangodbtests --gtest_filter="IResearchDocumentTest.*:*ReturnExecutor*"

Controlling the place where the test-data is stored:

    TMPDIR=/some/other/path ./scripts/unittest shell_server_aql

(Linux/Mac case. On Windows `TMP` or `TEMP` - as returned by `GetTempPathW` are the way to go)

Note that the `arangodbtests` executable is not compiled and shipped for
production releases (`-DUSE_GOOGLE_TESTS=off`).

`scripts/unittest` is only a wrapper for the most part, the backend
functionality lives in `js/client/modules/@arangodb/testutils`.
The actual testsuites are located in the
`js/client/modules/@arangodb/testsuites` folder.

#### Passing Options

The first parameter chooses the facility to execute. Available choices include:

- **single_client**: (see [Running a single unittest suite](#running-a-single-unittest-suite))
- **single_server**: (see [Running a single unittest suite](#running-a-single-unittest-suite))

Different facilities may take different options. The above mentioned usage
output contains the full detail.

Instead of starting its own instance, `unittest` can also make use of a
previously started arangod instance. You can launch the instance as you want
including via a debugger or `rr` and prepare it for what you want to test with
it. You then launch the test on it like this (assuming the default endpoint):

    ./scripts/unittest http_server --server tcp://127.0.0.1:8529/

A commandline for running a single test (-> with the facility 'single_server')
using valgrind could look like this. Options are passed as regular long values
in the syntax --option value --sub:option value. Using Valgrind could look like
this:

    ./scripts/unittest shell_client --test tests/js/server/aql/aql-escaping.js \
      --cluster true \
      --extraArgs:server.threads 1 \
      --extraArgs:scheduler.threads 1 \
      --extraArgs:javascript.gc-frequency 1000000 \
      --extraArgs:javascript.gc-interval 65536 \
      --extraArgs:agent.log.level trace \
      --extraArgs:log.level request=debug \
      --extraArgs:log.force-direct true \
      --javascript.v8-contexts 2 \
      --valgrind /usr/bin/valgrind \
      --valgrindargs:log-file /tmp/valgrindlog.%p

- We specify the test to execute
- We specify some arangod arguments via --extraArgs which increase the server performance
- We specify to run using valgrind (this is supported by all facilities)
- We specify some valgrind commandline arguments
- We set the log levels for agents to `trace` (the Instance type can be specified by:
  - `single`
  - `agent`
  - `dbserver`
  - `coordinator`
  - `activefailover`
  )
- We set the `requests` log level to debug on all instances
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

    scripts/unittest single_client --test tests/js/client/shell/shell-client.js

Running a test against a server you started (instead of letting the script start its own server):

    scripts/unittest shell_client --test api-batch.js --server tcp://127.0.0.1:8529 --serverRoot /tmp/123

Re-running previously failed tests:

    scripts/unittest <args> --failed

Specifying a `--test `-Filter containing `-cluster` will implicitely set `--cluster true` and launch a cluster test.

The `<args>` should be the same as in the previous run, only `--test`/`--testCase` can be omitted.
The information which tests failed is taken from the `UNITTEST_RESULT.json` in your test output folder.
This failed filter should work for all jsunity and mocha tests.

#### Running several Suites in one go

Several testsuites can be launched consequently in one run by specifying them as coma separated list.
They all share the specified commandline arguments. Individual arguments can be passed as a JSON array.
The JSON Array has to contain the same number of elements as testsuites specified. The specified individual
parameters will overrule global and default values.

Running the same testsuite twice with different and shared parameters would look like this:

    ./scripts/unittest  shell_client_multi,shell_client_multi --test shell-admin-status.js  --optionsJson '[{"http2":true,"suffix":"http2"},{"vst":true,"suffix":"vst"}]'


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
[mochajs.org](https://mochajs.org) framework. To run those tests, e.g. in the
arangosh, use this:

    require("@arangodb/mocha-runner").runTest('tests/js/client/endpoint-spec.js', true)

### Release tests

[RTA](https://github.com/arangodb/release-test-automation) has some tests to verify production integration.
To aid their development, they can also be used from the ArangoDB source tree.

#### MakeData / CheckData suite

The [makedata framework](https://github.com/arangodb/rta-makedata) as git submodule in [3rdParty/rta-makedata](3rdParty/rta-makedata/)
is implemented in arangosh javascript.
It uses the respective interface to execute DDL and DML operations. 
It facilitates a per database approach, and can be run multiple times in loops. 
It has hooks, that are intended to create DDL/DML objects in a way their existence
can be revalidated later on by other script hooks. The check hooks must respect a
flag whether they can create resources or whether they're talking to a read-only source.
Thus, the check-hooks may create data, but must remove it on success.
The checks should be considered not as time intense as the creation of the data.

It is used by RTA to:

- revalidate data can be created
- data survives hot backup / restore
- data makes it across the replication
- data is accessible with clusters with one failing DB-Server
- data makes it across DC2DC replication

With this integration additional DDL/DML checks can be more easily be developed without the
rather time and resource consuming and complex RTA framework.

The `rta_makedata` testsuite can be invoked with:

- `--cluster false` - to be ran on a single server setup.
- `--activefailover true` to be ran on an active failover setup.
- `--cluster true` to be ran on a 3 db-server node cluster; one run will check resilience with 2 remaining dbservers.

These combinations are also engaged via [test-definitions.txt](tests/test-definitions.txt).

Invoke it like this:

    ./scripts/unittest rta_makedata --cluster true

(you can override the 3rdParty/rta-makedata with `--rtasource ../rta-makedata` ,if you want to work with a full git clone of RTA-makedata)

### Driver tests

#### Go driver

Pre-requisites:

- have a go-driver checkout next to the ArangoDB source tree
- have the go binary in the path

Once this is completed, you may run it like this:

    ./scripts/unittest go_driver --gosource ../go-driver/ --testCase View --goOptions:timeout 180m --cluster true

This will invoke the test with a filter to only execute tests that have `View` in their name.
As an additional parameter we pass `-timeout 100m` to the driver test.

The driver integration also features JWT pass in. It will launch a cluster with 3 DB-Servers, as
the tests expect to have at least 3 DB-Servers.

#### Java driver

Pre-requisites:

- have a arangodb-java-driver checkout next to the ArangoDB source tree in the 'next' branch
- have a maven binary in the path (mvn) configured to use JDK 11

You can check if maven is correctly configured to use JDK 11 executing: `mvn -version`.
In case you have multiple JVMs in your machine and JDK 11 is not the default one, you can set
the environment variable `JAVA_HOME` to the root path of JDK 11.

Once this is completed, you may run it like this:

    ./scripts/unittest java_driver --javasource ../arangodb-java-driver/ \
        --javaOptions:failIfNoTests false \
        --testCase com.arangodb.next.api.collection.CollectionApiTest#countAndDropCollection \
        --cluster true

For possible `javaOptions` see
[arangodb-java-driver/dev-README.md#test-provided-deployment](https://github.com/arangodb/arangodb-java-driver/blob/master/dev-README.md)
in the java source, or the
[surefire documentation](https://maven.apache.org/surefire/maven-surefire-plugin/examples/single-test.html)

#### ArangoJS

Pre-requisites:

- have a arangojs checkout next to the ArangoDB source tree
- have a nodejs and yarn binaries installed and in the path
- ran `yarn` once in the arangojs source tree

Once this is completed, you may run it like this:

    ./scripts/unittest js_driver --jssource ../arangojs/ \
        --testCase 'kills the given query' \
        --cluster true

#### arangodb-php

Pre-requisites:

- have a arangodb-php checkout next to the ArangoDB source tree
- have `php` and `phpunit` binaries installed and in the path

At the time being phpunit version 6.5 is supported. Install it like this:

    wget "https://phar.phpunit.de/phpunit-6.5.14.phar"
    mv phpunit-6.5.14.phar /usr/bin/phpunit
    chmod a+x /usr/bin/phpunit

Once this is completed, you may run it like this:

    ./scripts/unittest php_driver --phpsource ../arangodb-php/ \
        --testCase testSaveVerticesAndEdgeBetweenThemAndRemoveOneByOne \
        --cluster true \
        --phpkeepalive false

(without connection keepalive)

#### generic driver interface

The generic driver interface expects to find i.e. script inside the
driver source, that does all the plumbing to run the respective tests against
the provided arangodb instance.
The invoked script is expected to exit non-zero on failure.
All content of `stdout` will be forwarded to the testresults.
All required data is passed as parameters:

- driversource - the source directory with the workingcopy of the driver
- driverScript - the script to be executed. defaults to `run_tests.sh`
- driverScriptInterpreter - since currently there is no shebang support,
  this needs to be provided or defaults to `/bin/bash`.
- driverOptions options to be passed on to the driver works in the form of
  `--driverOptions.argname value` evaluating to `--argname` `value`
- `--test testcase` evaluates to `--testsuite testcase`
- `--testCase testcaseExp` evaluates to `--filter testcaseExp`

Statically provided options (with sample values):

- `--instanceUrl http://127.0.0.1:7327`
- `--instanceEndpoint tcp://127.0.0.1:7327`
- `--port 7327`
- `--host 127.0.0.1`
- `--auth false`
- `--username root`
- `--password ''`
- `--[no-]enterprise`
- `--deployment-mode [SINGLE_SERVER|ACTIVE_FAILOVER|CLUSTER]`

### Debugging Tests

This is quick introduction only.

Running a single arangosh client test:

    ./scripts/unittest shell_client --test api-import.js

Debugging arangosh with gdb:

    server> ./scripts/unittest shell_client --test api-import.js --server tcp://127.0.0.1:7777

    client> gdb --args ./build/bin/arangod --server.endpoint http+tcp://127.0.0.1:6666 \
                                           --server.authentication false \
                                           --log.level communication=trace \
                                           ../arangodb-data-test

Debugging a storage engine:

    host> rm -fr ../arangodb-data-rocksdb/; \
       gdb --args ./build/bin/arangod \
           --console \
           --foxx.queues false \
           --server.statistics false \
           --server.endpoint http+tcp://0.0.0.0:7777 \
           ../arangodb-data-rocksdb
    (gdb) catch throw
    (gdb) r
    arangod> require("jsunity").runTest("tests/js/client/shell/shell-client.js");

### Forcing downgrade from VPack to JSON

While velocypack is better for the machine to machine communication, JSON does a better job
if you want to observe the communication using `tcpdump`.
Hence a downgrade of the communication to JSON can be made at start time:

    arangosh --server.force-json true --server.endpoint ...

### Running tcpdump / windump for the SUT

Don't want to miss a beat of your test? If you want to invoke tcpdump with sudo,
make sure that your current shell has sudo enabled. Try like this:

    sudo /bin/true; ./scripts/unittest http_server \
      --sniff sudo --cleanup false

The pcap file will end up in your tests temporary directory. You may need to
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
      --sniffDevice 1 \
      --sniffProgram 'c:/Program Files/wireshark/tshark.exe' \
      --forceJson true

You can later on use Wireshark to inspect the capture files.
(please note that `--forceJson` will downgrade the communication VPACK->JSON for better readability)

### Evaluating json test reports from previous testruns

All test results of testruns are dumped to a json file named
`UNITTEST_RESULT.json` which can be used for later analyzing of timings etc.

Currently available Analyzers are:

- unitTestPrettyPrintResults - Prints a pretty summary and writes an ASCII representation into `out/testfailures.txt` (if any errors)
- saveToJunitXML - saves jUnit compatible XML files
- locateLongRunning - searches the 10 longest running tests from a testsuite; may add comparison to `--otherFile` specified times
- locateShortServerLife - whether the servers lifetime for the tests isn't at least 10 times as long as startup/shutdown
- locateLongSetupTeardown - locate tests that may use a lot of time in setup/teardown
- yaml - dumps the json file as a yaml file
- unitTestTabularPrintResults - prints a table, add one (or more) of the following columns to print by adding it to `--tableColumns`:

  - `duration` - the time spent in the complete testfile
  - `status` - success/fail
  - `failed` - fail?
  - `total` - the time spent in the testcase
  - `totalSetUp` - the time spent in setup summarized
  - `totalTearDown` - the time spent in teardown summarized
  - `processStats.sum_servers.minorPageFaults` - Delta run values from `/proc/<pid>/io` summarized over all instances
  - `processStats.sum_servers.majorPageFaults` -
  - `processStats.sum_servers.userTime` -
  - `processStats.sum_servers.systemTime` -
  - `processStats.sum_servers.numberOfThreads` -
  - `processStats.sum_servers.residentSize` -
  - `processStats.sum_servers.residentSizePercent` -
  - `processStats.sum_servers.virtualSize` -
  - `processStats.sum_servers.rchar` -
  - `processStats.sum_servers.wchar` -
  - `processStats.sum_servers.syscr` -
  - `processStats.sum_servers.syscw` -
  - `processStats.sum_servers.read_bytes` -
  - `processStats.sum_servers.write_bytes` -
  - `processStats.sum_servers.cancelled_write_bytes` -
  - `processStats.sum_servers.sockstat_sockets_used` - Absolute values from `/proc/<pid>/net/sockstat` summarized over all instances
  - `processStats.sum_servers.sockstat_TCP_inuse` -
  - `processStats.sum_servers.sockstat_TCP_orphan` -
  - `processStats.sum_servers.sockstat_TCP_tw` -
  - `processStats.sum_servers.sockstat_TCP_alloc` -
  - `processStats.sum_servers.sockstat_TCP_mem` -
  - `processStats.sum_servers.sockstat_UDP_inuse` -
  - `processStats.sum_servers.sockstat_UDP_mem` -
  - `processStats.sum_servers.sockstat_UDPLITE_inuse` -
  - `processStats.sum_servers.sockstat_RAW_inuse` -
  - `processStats.sum_servers.sockstat_FRAG_inuse` -
  - `processStats.sum_servers.sockstat_FRAG_memory` -

  Process stats are kept by process.
  So if your DB-Server had the PID `1721882`, you can dial its values by specifying
  `processStats.1721882_dbserver.sockstat_TCP_tw`
  into the generated table.

i.e.

    ./scripts/examine_results.js -- 'yaml,locateLongRunning' --readFile out/UNITTEST_RESULT.json

or:

    ./scripts/examine_results.js -- 'unitTestTabularPrintResults' \
       --readFile out/UNITTEST_RESULT.json \
       --tableColumns 'duration,processStats.sum_servers.sockstat_TCP_orphan,processStats.sum_servers.sockstat_TCP_tw

revalidating one testcase using jq:

    jq '.shell_client."enterprise/tests/js/common/shell/smart-graph-enterprise-cluster.js"' < \
      out/UNITTEST_RESULT.json |grep sockstat_TCP_tw

getting the PIDs of the server in the testrun using jq:

    jq '.shell_client."enterprise/tests/js/common/shell/smart-graph-enterprise-cluster.js"' < \
      out/UNITTEST_RESULT.json |grep '"[0-9]*_[agent|dbserver|coordinator]'
    "1721674_agent": {
    "1721675_agent": {
    "1721676_agent": {
    "1721882_dbserver": {
    "1721883_dbserver": {
    "1721884_dbserver": {
    "1721885_coordinator": {

# Additional Resources

- [ArangoDB website](https://www.arangodb.com/)

- [ArangoDB on Twitter](https://twitter.com/arangodb)

- [General GitHub documentation](https://help.github.com/)

- [GitHub pull request documentation](https://docs.github.com/en/github/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork/)
