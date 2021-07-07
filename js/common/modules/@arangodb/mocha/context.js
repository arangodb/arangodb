'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

 class Context {
  runnable(runnable) {
    if (!arguments.length) {
      return this._runnable;
    }
    this.test = this._runnable = runnable;
    return this;
  }

  timeout(ms) {
    if (!arguments.length) {
      return this.runnable().timeout();
    }
    this.runnable().timeout(ms);
    return this;
  }

  enableTimeouts(enabled) {
    if (!arguments.length) {
      return this.runnable().enableTimeouts();
    }
    this.runnable().enableTimeouts(enabled);
    return this;
  }

  slow(ms) {
    if (!arguments.length) {
      return this.runnable().slow();
    }
    this.runnable().slow(ms);
    return this;
  }

  skip() {
    this.runnable().skip();
  }

  retries(n) {
    if (!arguments.length) {
      return this.runnable().retries();
    }
    this.runnable().retries(n);
    return this;
  }

}

module.exports = Context;