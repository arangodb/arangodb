'use strict';

const Runnable = require('./runnable');

class Hook extends Runnable {
/**
 * Initialize a new `Hook` with the given `title` and callback `fn`
 *
 * @class
 * @extends Runnable
 * @param {String} title
 * @param {Function} fn
 */
constructor(title, fn) {
  super(title, fn);
  this.type = 'hook';
}

/**
 * Get or set the test `err`.
 *
 * @memberof Hook
 * @public
 * @param {Error} err
 * @return {Error}
 */
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