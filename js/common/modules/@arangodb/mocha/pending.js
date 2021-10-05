'use strict';
// Based on mocha v6.1.3 under the MIT license.
// Original copyright (c) 2011-2018 JS Foundation and contributors,
// https://js.foundation

class Pending {
  constructor(message) {
    this.message = message;
  }
}

module.exports = Pending;