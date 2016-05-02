## Using the Console Test Runner

The console test runner is used to test browserless implementations of ECMAScript, e.g., [v8](http://en.wikipedia.org/wiki/V8_(JavaScript_engine)), [node](http://en.wikipedia.org/wiki/Node.js), or [js24](http://packages.ubuntu.com/trusty/libmozjs-24-bin) 

### Requirements

To use the `test262.py` runner, you must have the following:

 * a checkout of the [test262 project](https://github.com/tc39/test262/)
 * Python 2.7
 * the Python YAML library [PyYAML](http://www.pyyaml.org)
 * the javascript engine you intend to test (node, v8, etc.)

### Quick Start

To confirm the console test runner is working on a UNIX-like system

```
test262$ ./tools/packaging/test262.py --command "node" 7.2_A1.1_T1
ch07/7.2/S7.2_A1.1_T1 passed in non-strict mode

test262$
```

On a Windows system:

```
Z:\test262>tools\packaging\test262.py --command="node" 7.2_A1.1_T1
ch07\7.2\S7.2_A1.1_T1 passed in non-strict mode


Z:\test262>
```

If this does not work, see Troubleshooting (below)

### Options

Name | Action
-----|-------
-h, --help | displays a brief help message
--command=COMMAND | **required** command which invokes javascript engine to be tested
--tests=TESTS | path to the test suite; default is current directory
--cat | don't execute tests, just print code that would be run
--summary | generate a summary at end of execution
--full-summary | generate a longer summary with details of test failures
--strict_only | run only tests that are marked **onlyStrict**
--non_strict_only | run only tests that are marked **noStrict**
--unmarked_default=MODE | mode to use for tests that are not marked **onlyStrict** or **noStrict** ; MODE can be `strict` or `non_strict` or `both`
--logname=LOGNAME | write output to file (in addition to stdout)
--junitname=JUNITNAME | write test results to file in JUnit XML format
--loglevel=LOGLEVEL | set log level, primarily useful for debugging `test262.py` 
--print-handle=FUNC | enable async test logging via javascript function e.g., `console.log`
 
### Usage Notes

Non-option arguments are used as filters to match test names.  If no filters are found, the whole test suite is run.

Example | Result
-|-
test262.py --command="node" | run all tests
test262.py --command="node" ch07 ch11 | run tests from chapters 7 and 11
test262.py --command="node" 4.4 | run all tests with "4.4" in the name

The COMMAND argument can be a quoted string.  This is useful when testing ECMAScript6 features in node, because node requires the command-line argument `--harmony` to enable ES6:

```
$ test262.py --command="node --harmony" es6
``` 

#### Async Tests

Async tests require a 'print' function to be supplied to the test runner.  Here are some good defaults:

Engine | Filename | Print Function
-------|----------|---------------
V8/Node | node | console.log
V8/shell | shell | print
SpiderMonkey<sup>1</sup> | js | print
JavaScriptCore<sup>2</sup> | jsc | print

***Notes:***
1. As of 2014-Jul-23, SpiderMonkey does not support Promise in the `js` executable ([see bug 911216](https://bugzilla.mozilla.org/show_bug.cgi?id=911216) )
2. As of 2014-Jul-23, JavaScriptCore does not support Promise in the `jsc` executable 


### Troubleshooting

#### ImportError: No module named yaml

On Windows, the message "No module named yaml" can mean that the PyYAML library is installed but not found.  If you have this problem, you may be able to use `yaml` interactively from python:

```
Z:\Code\github\test262>python
ActivePython 2.7.5.6 (ActiveState Software Inc.) based on
Python 2.7.5 (default, Sep 16 2013, 23:16:52) [MSC v.1500 32 bit (Intel)] on win
32
Type "help", "copyright", "credits" or "license" for more information.
>>> import yaml
>>> yaml
<module 'yaml' from 'C:\Python27\lib\site-packages\yaml\__init__.pyc'>
```

If you can load `yaml` interactively but get the `ImportError` when running `test262.py`:

```
Z:\Code\github\test262>tools\packaging\test262.py --command="node --harmony" --p
rint-handle="console.log" ch25
Traceback (most recent call last):
  File "Z:\Code\github\test262\tools\packaging\test262.py", line 31, in <module>

    from parseTestRecord import parseTestRecord, stripHeader
  File "Z:\Code\github\test262\tools\packaging\parseTestRecord.py", line 20, in
<module>
    import yaml
ImportError: No module named yaml
```

Then the fix is to explicitly set the PYTHONPATH environment variable.  The location may vary from system to system, but it is typically `'C:\Python27\lib\site-packages`.

```
set PYTHONPATH=C:\Python27\lib\site-packages
```
