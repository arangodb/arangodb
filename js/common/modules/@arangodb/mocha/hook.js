'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

const Runnable = require('@arangodb/mocha/runnable');

class Hook extends Runnable {
  constructor(title, fn) {
    super(title, fn);
    this.type = 'hook';
  }

  error(err) {
    if (!arguments.length) {
      err = this._error;
      this._error = null;
      return err;
    }

    this._error = err;
  }
}

module.exports = Hook;