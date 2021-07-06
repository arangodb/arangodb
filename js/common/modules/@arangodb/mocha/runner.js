'use strict';

/**
 * Module dependencies.
 */
const util = require('util');
const EventEmitter = require('events').EventEmitter;
const Pending = require('./pending');
const Runnable = require('./runnable');
const Suite = require('./suite');

function createInvalidExceptionError(message, value) {
  var err = new Error(message);
  err.code = 'ERR_MOCHA_INVALID_EXCEPTION';
  err.valueType = typeof value;
  err.value = value;
  return err;
}

/**
 * Non-enumerable globals.
 * @readonly
 */
var globals = [
  'setTimeout',
  'clearTimeout',
  'setInterval',
  'clearInterval',
  'XMLHttpRequest',
  'Date',
  'setImmediate',
  'clearImmediate'
];

class Runner extends EventEmitter {

/**
 * Initialize a `Runner` at the Root {@link Suite}, which represents a hierarchy of {@link Suite|Suites} and {@link Test|Tests}.
 *
 * @extends external:EventEmitter
 * @public
 * @class
 * @param {Suite} suite Root suite
 * @param {boolean} [delay] Whether or not to delay execution of root suite
 * until ready.
 */
constructor(suite, delay) {
  super();
  var self = this;
  this._globals = [];
  this._abort = false;
  this._delay = delay;
  this.suite = suite;
  this.started = false;
  this.total = suite.total();
  this.failures = 0;
  this.on('test end', function(test) {
    self.checkGlobals(test);
  });
  this.on('hook end', function(hook) {
    self.checkGlobals(hook);
  });
  this._defaultGrep = /.*/;
  this.grep(this._defaultGrep);
  this.globals(this.globalProps().concat(extraGlobals()));
}

/**
 * Run tests with full titles matching `re`. Updates runner.total
 * with number of tests matched.
 *
 * @public
 * @memberof Runner
 * @param {RegExp} re
 * @param {boolean} invert
 * @return {Runner} Runner instance.
 */
grep(re, invert) {
  this._grep = re;
  this._invert = invert;
  this.total = this.grepTotal(this.suite);
  return this;
}

/**
 * Returns the number of tests matching the grep search for the
 * given suite.
 *
 * @memberof Runner
 * @public
 * @param {Suite} suite
 * @return {number}
 */
grepTotal(suite) {
  var self = this;
  var total = 0;

  suite.eachTest(function(test) {
    var match = self._grep.test(test.fullTitle());
    if (self._invert) {
      match = !match;
    }
    if (match) {
      total++;
    }
  });

  return total;
}

/**
 * Return a list of global properties.
 *
 * @return {Array}
 * @private
 */
globalProps() {
  var props = Object.keys(global);

  // non-enumerables
  for (var i = 0; i < globals.length; ++i) {
    if (~props.indexOf(globals[i])) {
      continue;
    }
    props.push(globals[i]);
  }

  return props;
}

/**
 * Allow the given `arr` of globals.
 *
 * @public
 * @memberof Runner
 * @param {Array} arr
 * @return {Runner} Runner instance.
 */
globals(arr) {
  if (!arguments.length) {
    return this._globals;
  }
  this._globals = this._globals.concat(arr);
  return this;
}

/**
 * Check for global variable leaks.
 *
 * @private
 */
checkGlobals(test) {
  if (this.ignoreLeaks) {
    return;
  }
  var ok = this._globals;

  var globals = this.globalProps();
  var leaks;

  if (test) {
    ok = ok.concat(test._allowedGlobals || []);
  }

  if (this.prevGlobalsLength === globals.length) {
    return;
  }
  this.prevGlobalsLength = globals.length;

  leaks = filterLeaks(ok, globals);
  this._globals = this._globals.concat(leaks);

  if (leaks.length) {
    var format = (
      leaks.length === 1
      ? 'global leak detected: %s'
      : 'global leaks detected: %s'
    );
    var error = new Error(util.format(format, leaks.map(leak => `'${leak}'`).join(', ')));
    this.fail(test, error);
  }
}

/**
 * Fail the given `test`.
 *
 * @private
 * @param {Test} test
 * @param {Error} err
 */
fail(test, err) {
  if (test.isPending()) {
    return;
  }

  ++this.failures;
  test.state = 'failed';

  if (!isError(err)) {
    err = thrown2Error(err);
  }

  // TODO filter stack trace

  this.emit('fail', test, err);
}

/**
 * Fail the given `hook` with `err`.
 *
 * Hook failures work in the following pattern:
 * - If bail, run corresponding `after each` and `after` hooks,
 *   then exit
 * - Failed `before` hook skips all tests in a suite and subsuites,
 *   but jumps to corresponding `after` hook
 * - Failed `before each` hook skips remaining tests in a
 *   suite and jumps to corresponding `after each` hook,
 *   which is run only once
 * - Failed `after` hook does not alter
 *   execution order
 * - Failed `after each` hook skips remaining tests in a
 *   suite and subsuites, but executes other `after each`
 *   hooks
 *
 * @private
 * @param {Hook} hook
 * @param {Error} err
 */
failHook(hook, err) {
  hook.originalTitle = hook.originalTitle || hook.title;
  if (hook.ctx && hook.ctx.currentTest) {
    hook.title =
      hook.originalTitle + ' for ' + `"hook.ctx.currentTest.title"`;
  } else {
    var parentTitle;
    if (hook.parent.title) {
      parentTitle = hook.parent.title;
    } else {
      parentTitle = hook.parent.root ? '{root}' : '';
    }
    hook.title = hook.originalTitle + ' in ' + `"parentTitle"`;
  }

  this.fail(hook, err);
}

/**
 * Run hook `name` callbacks and then invoke `fn()`.
 *
 * @private
 * @param {string} name
 * @param {Function} fn
 */

hook(name, fn) {
  var suite = this.suite;
  var hooks = suite.getHooks(name);
  var self = this;

  function next(i) {
    var hook = hooks[i];
    if (!hook) {
      return fn();
    }
    self.currentRunnable = hook;

    if (name === 'beforeAll') {
      hook.ctx.currentTest = hook.parent.tests[0];
    } else if (name === 'afterAll') {
      hook.ctx.currentTest = hook.parent.tests[hook.parent.tests.length - 1];
    } else {
      hook.ctx.currentTest = self.test;
    }

    hook.allowUncaught = self.allowUncaught;

    self.emit('hook', hook);

    if (!hook.listeners('error').length) {
      hook.on('error', function(err) {
        self.failHook(hook, err);
      });
    }

    hook.run(function(err) {
      var testError = hook.error();
      if (testError) {
        self.fail(self.test, testError);
      }
      if (err) {
        if (err instanceof Pending) {
          if (name === 'afterAll') {
            throw new Error('Skipping a test within an "after all" hook is forbidden.');
          }
          if (name === 'beforeEach' || name === 'afterEach') {
            if (self.test) {
              self.test.pending = true;
            }
          } else {
            suite.tests.forEach(function(test) {
              test.pending = true;
            });
            suite.suites.forEach(function(suite) {
              suite.pending = true;
            });
            // a pending hook won't be executed twice.
            hook.pending = true;
          }
        } else {
          self.failHook(hook, err);

          // stop executing hooks, notify callee of hook err
          return fn(err);
        }
      }
      self.emit('hook end', hook);
      delete hook.ctx.currentTest;
      next(++i);
    });
  }

  next(0);
}

/**
 * Run hook `name` for the given array of `suites`
 * in order, and callback `fn(err, errSuite)`.
 *
 * @private
 * @param {string} name
 * @param {Array} suites
 * @param {Function} fn
 */
hooks(name, suites, fn) {
  var self = this;
  var orig = this.suite;

  function next(suite) {
    self.suite = suite;

    if (!suite) {
      self.suite = orig;
      return fn();
    }

    self.hook(name, function(err) {
      if (err) {
        var errSuite = self.suite;
        self.suite = orig;
        return fn(err, errSuite);
      }

      next(suites.pop());
    });
  }

  next(suites.pop());
}

/**
 * Run hooks from the top level down.
 *
 * @param {String} name
 * @param {Function} fn
 * @private
 */
hookUp(name, fn) {
  var suites = [this.suite].concat(this.parents()).reverse();
  this.hooks(name, suites, fn);
}

/**
 * Run hooks from the bottom up.
 *
 * @param {String} name
 * @param {Function} fn
 * @private
 */
hookDown(name, fn) {
  var suites = [this.suite].concat(this.parents());
  this.hooks(name, suites, fn);
}

/**
 * Return an array of parent Suites from
 * closest to furthest.
 *
 * @return {Array}
 * @private
 */
parents() {
  var suite = this.suite;
  var suites = [];
  while (suite.parent) {
    suite = suite.parent;
    suites.push(suite);
  }
  return suites;
}

/**
 * Run the current test and callback `fn(err)`.
 *
 * @param {Function} fn
 * @private
 */
runTest(fn) {
  var self = this;
  var test = this.test;

  if (!test) {
    return;
  }

  var suite = this.parents().reverse()[0] || this.suite;
  if (this.forbidOnly && suite.hasOnly()) {
    fn(new Error('`.only` forbidden'));
    return;
  }
  test.on('error', function(err) {
    self.fail(test, err);
  });
  if (this.allowUncaught) {
    test.allowUncaught = true;
    return test.run(fn);
  }
  try {
    test.run(fn);
  } catch (err) {
    fn(err);
  }
}

/**
 * Run tests in the given `suite` and invoke the callback `fn()` when complete.
 *
 * @private
 * @param {Suite} suite
 * @param {Function} fn
 */
runTests(suite, fn) {
  var self = this;
  var tests = suite.tests.slice();
  var test;

  function hookErr(_, errSuite, after) {
    // before/after Each hook for errSuite failed:
    var orig = self.suite;

    // for failed 'after each' hook start from errSuite parent,
    // otherwise start from errSuite itself
    self.suite = after ? errSuite.parent : errSuite;

    if (self.suite) {
      // call hookUp afterEach
      self.hookUp('afterEach', function(err2, errSuite2) {
        self.suite = orig;
        // some hooks may fail even now
        if (err2) {
          return hookErr(err2, errSuite2, true);
        }
        // report error suite
        fn(errSuite);
      });
    } else {
      // there is no need calling other 'after each' hooks
      self.suite = orig;
      fn(errSuite);
    }
  }

  function next(err, errSuite) {
    // if we bail after first err
    if (self.failures && suite._bail) {
      tests = [];
    }

    if (self._abort) {
      return fn();
    }

    if (err) {
      return hookErr(err, errSuite, true);
    }

    // next test
    test = tests.shift();

    // all done
    if (!test) {
      return fn();
    }

    // grep
    var match = self._grep.test(test.fullTitle());
    if (self._invert) {
      match = !match;
    }
    if (!match) {
      next();
      return;
    }

    if (test.isPending()) {
      if (self.forbidPending) {
        test.isPending = () => false;
        self.fail(test, new Error('Pending test forbidden'));
        delete test.isPending;
      } else {
        self.emit('pending', test);
      }
      self.emit('test end', test);
      return next();
    }

    // execute test and hook(s)
    self.emit('test', (self.test = test));
    self.hookDown('beforeEach', function(err, errSuite) {
      if (test.isPending()) {
        if (self.forbidPending) {
          test.isPending = () => false;
          self.fail(test, new Error('Pending test forbidden'));
          delete test.isPending;
        } else {
          self.emit('pending', test);
        }
        self.emit('test end', test);
        return next();
      }
      if (err) {
        return hookErr(err, errSuite, false);
      }
      self.currentRunnable = self.test;
      self.runTest(function(err) {
        test = self.test;
        if (err) {
          var retry = test.currentRetry();
          if (err instanceof Pending && self.forbidPending) {
            self.fail(test, new Error('Pending test forbidden'));
          } else if (err instanceof Pending) {
            test.pending = true;
            self.emit('pending', test);
          } else if (retry < test.retries()) {
            var clonedTest = test.clone();
            clonedTest.currentRetry(retry + 1);
            tests.unshift(clonedTest);

            self.emit('retry', test, err);

            // Early return + hook trigger so that it doesn't
            // increment the count wrong
            return self.hookUp('afterEach', next);
          } else {
            self.fail(test, err);
          }
          self.emit('test end', test);

          if (err instanceof Pending) {
            return next();
          }

          return self.hookUp('afterEach', next);
        }

        test.state = 'passed';
        self.emit('pass', test);
        self.emit('test end', test);
        self.hookUp('afterEach', next);
      });
    });
  }

  this.next = next;
  this.hookErr = hookErr;
  next();
}

/**
 * Run the given `suite` and invoke the callback `fn()` when complete.
 *
 * @private
 * @param {Suite} suite
 * @param {Function} fn
 */
runSuite(suite, fn) {
  var i = 0;
  var self = this;
  var total = this.grepTotal(suite);
  var afterAllHookCalled = false;


  if (!total || (self.failures && suite._bail)) {
    return fn();
  }

  this.emit('suite', (this.suite = suite));

  function next(errSuite) {
    if (errSuite) {
      // current suite failed on a hook from errSuite
      if (errSuite === suite) {
        // if errSuite is current suite
        // continue to the next sibling suite
        return done();
      }
      // errSuite is among the parents of current suite
      // stop execution of errSuite and all sub-suites
      return done(errSuite);
    }

    if (self._abort) {
      return done();
    }

    var curr = suite.suites[i++];
    if (!curr) {
      return done();
    }

    self.runSuite(curr, next);
  }

  function done(errSuite) {
    self.suite = suite;
    self.nextSuite = next;

    if (afterAllHookCalled) {
      fn(errSuite);
    } else {
      // mark that the afterAll block has been called once
      // and so can be skipped if there is an error in it.
      afterAllHookCalled = true;

      // remove reference to test
      delete self.test;

      self.hook('afterAll', function() {
        self.emit('suite end', suite);
        fn(errSuite);
      });
    }
  }

  this.nextSuite = next;

  this.hook('beforeAll', function(err) {
    if (err) {
      return done();
    }
    self.runTests(suite, next);
  });
}

/**
 * Handle uncaught exceptions.
 *
 * @param {Error} err
 * @private
 */
uncaught(err) {
  if (err instanceof Pending) {
    return;
  }
  if (!err) {
    err = createInvalidExceptionError(
      'Caught falsy/undefined exception which would otherwise be uncaught. No stack trace found; try a debugger',
      err
    );
  }

  if (!isError(err)) {
    err = thrown2Error(err);
  }
  err.uncaught = true;

  var runnable = this.currentRunnable;

  if (!runnable) {
    runnable = new Runnable('Uncaught error outside test suite');
    runnable.parent = this.suite;

    if (this.started) {
      this.fail(runnable, err);
    } else {
      // Can't recover from this failure
      this.emit('start');
      this.fail(runnable, err);
      this.emit('end');
    }

    return;
  }

  // Ignore errors if already failed or pending
  // See #3226
  if (runnable.isFailed() || runnable.isPending()) {
    return;
  }
  // we cannot recover gracefully if a Runnable has already passed
  // then fails asynchronously
  var alreadyPassed = runnable.isPassed();
  // this will change the state to "failed" regardless of the current value
  this.fail(runnable, err);
  if (!alreadyPassed) {
    // recover from test
    if (runnable.type === 'test') {
      this.emit('test end', runnable);
      this.hookUp('afterEach', this.next);
      return;
    }

    // recover from hooks
    var errSuite = this.suite;

    // XXX how about a less awful way to determine this?
    // if hook failure is in afterEach block
    if (runnable.fullTitle().indexOf('after each') > -1) {
      return this.hookErr(err, errSuite, true);
    }
    // if hook failure is in beforeEach block
    if (runnable.fullTitle().indexOf('before each') > -1) {
      return this.hookErr(err, errSuite, false);
    }
    // if hook failure is in after or before blocks
    return this.nextSuite(errSuite);
  }

  // bail
  this.emit('end');
}

/**
 * Run the root suite and invoke `fn(failures)`
 * on completion.
 *
 * @public
 * @memberof Runner
 * @param {Function} fn
 * @return {Runner} Runner instance.
 */
run(fn) {
  var self = this;
  var rootSuite = this.suite;

  fn = fn || function() {}

  function uncaught(err) {
    self.uncaught(err);
  }

  function start() {
    // If there is an `only` filter
    if (rootSuite.hasOnly()) {
      rootSuite.filterOnly();
    }
    self.started = true;
    if (self._delay) {
      self.emit('ready');
    }
    self.emit('start');

    self.runSuite(rootSuite, function() {
      self.emit('end');
    });
  }


  // references cleanup to avoid memory leaks
  this.on('suite end', function(suite) {
    suite.cleanReferences();
  });

  // callback
  this.on('end', function() {
    process.removeListener('uncaughtException', uncaught);
    fn(self.failures);
  });

  // uncaught exception
  process.on('uncaughtException', uncaught);

  if (this._delay) {
    // for reporters, I guess.
    // might be nice to debounce some dots while we wait.
    this.emit('waiting', rootSuite);
    rootSuite.once('run', start);
  } else {
    start();
  }

  return this;
}

/**
 * Cleanly abort execution.
 *
 * @memberof Runner
 * @public
 * @return {Runner} Runner instance.
 */
abort() {
  this._abort = true;

  return this;
}
}

/**
 * Filter leaks with the given globals flagged as `ok`.
 *
 * @private
 * @param {Array} ok
 * @param {Array} globals
 * @return {Array}
 */
function filterLeaks(ok, globals) {
  return globals.filter(function(key) {
    // Firefox and Chrome exposes iframes as index inside the window object
    if (/^\d+/.test(key)) {
      return false;
    }

    // in firefox
    // if runner runs in an iframe, this iframe's window.getInterface method
    // not init at first it is assigned in some seconds
    if (global.navigator && /^getInterface/.test(key)) {
      return false;
    }

    // an iframe could be approached by window[iframeIndex]
    // in ie6,7,8 and opera, iframeIndex is enumerable, this could cause leak
    if (global.navigator && /^\d+/.test(key)) {
      return false;
    }

    // Opera and IE expose global variables for HTML element IDs (issue #243)
    if (/^mocha-/.test(key)) {
      return false;
    }

    var matched = ok.filter(function(ok) {
      if (~ok.indexOf('*')) {
        return key.indexOf(ok.split('*')[0]) === 0;
      }
      return key === ok;
    });
    return !matched.length && (!global.navigator || key !== 'onerror');
  });
}

/**
 * Check if argument is an instance of Error object or a duck-typed equivalent.
 *
 * @private
 * @param {Object} err - object to check
 * @param {string} err.message - error message
 * @returns {boolean}
 */
function isError(err) {
  return err instanceof Error || (err && typeof err.message === 'string');
}

/**
 *
 * Converts thrown non-extensible type into proper Error.
 *
 * @private
 * @param {*} thrown - Non-extensible type thrown by code
 * @return {Error}
 */
function thrown2Error(err) {
  return new Error(
    util.inspect(err) + ' was thrown, throw an Error :)'
  );
}

/**
 * Array of globals dependent on the environment.
 *
 * @return {Array}
 * @deprecated
 * @todo remove; long since unsupported
 * @private
 */
function extraGlobals() {
  if (typeof process === 'object' && typeof process.version === 'string') {
    var parts = process.version.split('.');
    var nodeVersion = parts.reduce(function(a, v) {
      return (a << 8) | v;
    });

    // 'errno' was renamed to process._errno in v0.9.11.
    if (nodeVersion < 0x00090b) {
      return ['errno'];
    }
  }

  return [];
}

module.exports = Runner;