/*jshint strict: false */
/*global require */
let other = require('@arangodb/test-helper-common');
Object.keys(other).forEach((o) => {
  exports[o] = other[o];
});
