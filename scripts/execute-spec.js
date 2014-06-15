/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true, todo: true, evil: true */
/*global module, require, exports, print */

/** Usage
 *
 * ./bin/arangod --log.level warning --javascript.script scripts/execute-spec.js --javascript.script-parameter js/server/tests/shell-example-spec.js --javascript.script-parameter js/server/tests/shell-example-spec-2.js /tmp/tests
 *
 */

function main(argv) {
  var jasmine = require('jasmine'),
    _ = require('underscore'),
    describe = jasmine.describe,
    it = jasmine.it,
    expect = jasmine.expect,
    fs = require('fs'),
    file,
    status;

  if (argv.length >= 2) {
    _.each(argv.slice(1), function (fileName) {
      file = fs.read(fileName);
      eval(file);
    });

    jasmine.execute();
    status = jasmine.status();
  } else {
    print('Provide exactly one filename');
    status = 1;
  }

  return status;
}
