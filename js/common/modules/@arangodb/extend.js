'use strict';
const extend = require('underscore').extend;

exports.extend = function (protoProps, staticProps) {
  const init = protoProps.constructor;
  const Class = class extends this {
    constructor(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) {
      super(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
      if (init) {
        init.apply(this, arguments);
      }
    }
  };
  extend(Class.prototype, protoProps);
  extend(Class, staticProps);
  return Class;
};
