"use strict";

var parserFactories = require('./parser-factories');

exports.darwin = parserFactories.darwin();
exports.win32 = parserFactories.win32();
exports.linux = parserFactories.linux({
  parseName: false,
});
