'use strict';
const extend = require('underscore').extend;
const deprecated = require('org/arangodb/deprecated');

exports.extend = function (protoProps, staticProps) {
  deprecated('2.9', '"extend" is deprecated, use class syntax instead');
  const Class = class extends this {};
  extend(Class.prototype, protoProps);
  extend(Class, staticProps);
  return Class;
};
