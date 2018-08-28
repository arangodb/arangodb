'use strict';
// this throws if config is not set (undefined has no method)
var value = module.context.configuration.stringValue.toLowerCase();
require('console').log(value);