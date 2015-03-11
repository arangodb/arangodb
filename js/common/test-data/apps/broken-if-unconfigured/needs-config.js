"use strict";
// this throws if config is not set (undefined has no method)
var value = applicationContext.configuration.stringValue.toLowerCase();
require('console').log(value);