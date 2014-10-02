'use strict';


var assert = require('assert');
var path   = require('path');
var fs     = require('fs');
var yaml   = require('../');

var TEST_SCHEMA = require('./support/schema').TEST_SCHEMA;


suite('Dumper', function () {
  var samplesDir = path.resolve(__dirname, 'samples-common');

  fs.readdirSync(samplesDir).forEach(function (jsFile) {
    if ('.js' !== path.extname(jsFile)) {
      return; // continue
    }

    var yamlFile = path.resolve(samplesDir, path.basename(jsFile, '.js') + '.yml');

    test(path.basename(jsFile, '.js'), function () {
      var sample       = require(path.resolve(samplesDir, jsFile)),
          data         = 'function' === typeof sample ? sample.expected : sample,
          serialized   = yaml.dump(data,       { schema: TEST_SCHEMA }),
          deserialized = yaml.load(serialized, { schema: TEST_SCHEMA });

      if ('function' === typeof sample) {
        sample.call(this, deserialized);
      } else {
        assert.deepEqual(deserialized, sample);
      }
    });
  });
});
