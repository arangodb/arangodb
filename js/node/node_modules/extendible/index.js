// 'use strict'; //<-- Root of all evil, causes thrown errors on readyOnly props.

var has = Object.prototype.hasOwnProperty
  , slice = Array.prototype.slice;

/**
 * Copy all readable properties from an Object or function and past them on the
 * object.
 *
 * @param {Object} obj The object we should paste everything on.
 * @returns {Object} obj
 * @api private
 */
function copypaste(obj) {
  var args = slice.call(arguments, 1)
    , i = 0
    , prop;

  for (; i < args.length; i++) {
    if (!args[i]) continue;

    for (prop in args[i]) {
      if (!has.call(args[i], prop)) continue;

      obj[prop] = args[i][prop];
    }
  }

  return obj;
}

/**
 * A proper mixin function that respects getters and setters.
 *
 * @param {Object} obj The object that should receive all properties.
 * @returns {Object} obj
 * @api private
 */
function mixin(obj) {
  if (
       'function' !== typeof Object.getOwnPropertyNames
    || 'function' !== typeof Object.defineProperty
    || 'function' !== typeof Object.getOwnPropertyDescriptor
  ) {
    return copypaste.apply(null, arguments);
  }

  //
  // We can safely assume that if the methods we specify above are supported
  // that it's also save to use Array.forEach for iteration purposes.
  //
  slice.call(arguments, 1).forEach(function forEach(o) {
    Object.getOwnPropertyNames(o).forEach(function eachAttr(attr) {
      Object.defineProperty(obj, attr, Object.getOwnPropertyDescriptor(o, attr));
    });
  });

  return obj;
}

/**
 * Detect if a given parent is constructed in strict mode so we can force the
 * child in to the same mode. It detects the strict mode by accessing properties
 * on the function that are forbidden in strict mode:
 *
 * - `caller`
 * - `callee`
 * - `arguments`
 *
 * Forcing the a thrown TypeError.
 *
 * @param {Function} parent Parent constructor
 * @returns {Function} The child constructor
 * @api private
 */
function mode(parent) {
  try {
    var e = parent.caller || parent.arguments || parent.callee;

    return function child() {
      return parent.apply(this, arguments);
    };
  } catch(e) {}

  return function child() {
    'use strict';

    return parent.apply(this, arguments);
  };
}

//
// Helper function to correctly set up the prototype chain, for subclasses.
// Similar to `goog.inherits`, but uses a hash of prototype properties and
// class properties to be extended.
//
module.exports = function extend(protoProps, staticProps) {
  var parent = this
    , child;

  //
  // The constructor function for the new subclass is either defined by you
  // (the "constructor" property in your `extend` definition), or defaulted
  // by us to simply call the parent's constructor.
  //
  if (protoProps && has.call(protoProps, 'constructor')) {
    child = protoProps.constructor;
  } else {
    child = mode(parent);
  }

  //
  // Set the prototype chain to inherit from `parent`, without calling
  // `parent`'s constructor function.
  //
  function Surrogate() {
    this.constructor = child;
  }

  Surrogate.prototype = parent.prototype;
  child.prototype = new Surrogate;

  //
  // Add prototype properties (instance properties) to the subclass,
  // if supplied.
  //
  if (protoProps) mixin(child.prototype, protoProps);

  //
  // Add static properties to the constructor function, if supplied.
  //
  copypaste(child, parent, staticProps);

  //
  // Set a convenience property in case the parent's prototype is needed later.
  //
  child.__super__ = parent.prototype;

  return child;
};
