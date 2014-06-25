/*jslint indent: 2, nomen: true, maxlen: 120, white: true, plusplus: true, unparam: true, regexp: true, vars: true */
/*global require, exports */
(function () {
  'use strict';

  function ProviderNotFound(key) {
    this.message = 'Provider with key ' + key + ' not found.';
  }
  ProviderNotFound.prototype = new Error();

  exports.ProviderNotFound = ProviderNotFound;
}());