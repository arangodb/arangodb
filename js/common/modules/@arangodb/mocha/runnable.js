'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const EventEmitter = require('events').EventEmitter;
const Pending = require('@arangodb/mocha/pending');
const milliseconds = require('ms');

function createInvalidExceptionError(message, value) {
  var err = new Error(message);
  err.code = 'ERR_MOCHA_INVALID_EXCEPTION';
  err.valueType = typeof value;
  err.value = value;
  return err;
}

function toValueOrError(value) {
  return (
    value ||
    createInvalidExceptionError(
      'Runnable failed with falsy or undefined exception. Please throw an Error instead.',
      value
    )
  );
}


var Date = global.Date;
var toString = Object.prototype.toString;

class Runnable extends EventEmitter {
  constructor(title, fn) {
    super();
    this.title = title;
    this.fn = fn;
    this.body = (fn || '').toString();
    this.async = fn && fn.length;
    this.sync = !this.async;
    this._timeout = 2000;
    this._slow = 75;
    this._enableTimeouts = true;
    this.timedOut = false;
    this._retries = -1;
    this._currentRetry = 0;
    this.pending = false;
  }

  timeout(ms) {
    if (!arguments.length) {
      return this._timeout;
    }
    if (typeof ms === 'string') {
      ms = milliseconds(ms);
    }

    var INT_MAX = Math.pow(2, 31) - 1;
    ms = Math.max(0, Math.min(ms, INT_MAX));

    if (ms === 0 || ms === INT_MAX) {
      this._enableTimeouts = false;
    }
    this._timeout = ms;
    return this;
  }

  slow(ms) {
    if (!arguments.length || typeof ms === 'undefined') {
      return this._slow;
    }
    if (typeof ms === 'string') {
      ms = milliseconds(ms);
    }
    this._slow = ms;
    return this;
  }

  enableTimeouts(enabled) {
    if (!arguments.length) {
      return this._enableTimeouts;
    }
    this._enableTimeouts = enabled;
    return this;
  }

  skip() {
    throw new Pending('sync skip');
  }

  isPending() {
    return this.pending || (this.parent && this.parent.isPending());
  }

  isFailed() {
    return !this.isPending() && this.state === 'failed';
  }

  isPassed() {
    return !this.isPending() && this.state === 'passed';
  }

  retries(n) {
    if (!arguments.length) {
      return this._retries;
    }
    this._retries = n;
  }

  currentRetry(n) {
    if (!arguments.length) {
      return this._currentRetry;
    }
    this._currentRetry = n;
  }

  fullTitle() {
    return this.titlePath().join(' ');
  }

  titlePath() {
    return this.parent.titlePath().concat([this.title]);
  }

  inspect() {
    return JSON.stringify(
      this,
      function(key, val) {
        if (key[0] === '_') {
          return;
        }
        if (key === 'parent') {
          return '#<Suite>';
        }
        if (key === 'ctx') {
          return '#<Context>';
        }
        return val;
      },
      2
    );
  }

  globals(globals) {
    if (!arguments.length) {
      return this._allowedGlobals;
    }
    this._allowedGlobals = globals;
  }

  run(fn) {
    var self = this;
    var start = new Date();
    var ctx = this.ctx;
    var finished;
    var emitted;

    if (ctx && ctx.runnable) {
      ctx.runnable(this);
    }

    function multiple(err) {
      if (emitted) {
        return;
      }
      emitted = true;
      var msg = 'done() called multiple times';
      if (err && err.message) {
        err.message += " (and Mocha's " + msg + ')';
        self.emit('error', err);
      } else {
        self.emit('error', new Error(msg));
      }
    }

    function done(err) {
      var ms = self.timeout();
      if (self.timedOut) {
        return;
      }

      if (finished) {
        return multiple(err);
      }

      self.duration = new Date() - start;
      finished = true;
      if (!err && self.duration > ms && self._enableTimeouts) {
        err = self._timeoutError(ms);
      }
      fn(err);
    }

    this.callback = done;

    if (this.async) {
      this.skip = function asyncSkip() {
        done(new Pending('async skip call'));
        throw new Pending('async skip; aborting execution');
      };

      if (this.allowUncaught) {
        return callFnAsync(this.fn);
      }
      try {
        callFnAsync(this.fn);
      } catch (err) {
        emitted = true;
        done(toValueOrError(err));
      }
      return;
    }

    if (this.allowUncaught) {
      if (!this.isPending()) {
        this.fn(ctx);
      }
      done();
      return;
    }

    try {
      if (!this.isPending()) {
        this.fn(ctx);
      }
      done();
    } catch (err) {
      emitted = true;
      done(toValueOrError(err));
    }

    function callFnAsync(fn) {
      fn.call(ctx, function(err) {
        if (err instanceof Error || toString.call(err) === '[object Error]') {
          return done(err);
        }
        if (err) {
          if (Object.prototype.toString.call(err) === '[object Object]') {
            return done(
              new Error('done() invoked with non-Error: ' + JSON.stringify(err))
            );
          }
          return done(new Error('done() invoked with non-Error: ' + err));
        }
        done();
      });
    }
  }

  _timeoutError(ms) {
    var msg =
      'Timeout of ' +
      ms +
      'ms exceeded.';
    if (this.file) {
      msg += ' (' + this.file + ')';
    }
    return new Error(msg);
  }
}

module.exports = Runnable;