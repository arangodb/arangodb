/* jshint globalstrict: true, es3: true */
/* globals require: false, module: false, console: false */
'use strict';
var AqlError = require('./errors').AqlError,
  assumptions = require('./assumptions'),
  types = require('./types'),
  QB = {},
  bind,
  toArray,
  warn;


bind = (typeof Function.prototype.bind === 'function' ? (
  Function.prototype.call.bind(Function.prototype.bind)
) : (
  function (fn, self) {
    return function () {
      return fn.apply(self, arguments);
    };
  }
));

toArray = bind(Function.prototype.call, Array.prototype.slice);

warn = (function () {
  if (typeof console !== 'undefined') {
    return function () {return console.warn.apply(console, arguments);};
  }
  try {
    var cons = require('console');
    return function () {return cons.warn.apply(cons, arguments);};
  } catch (err) {
    return function () {};
  }
}());

for (var key in types._PartialStatement.prototype) {
  if (types._PartialStatement.prototype.hasOwnProperty(key) && key !== 'constructor') {
    QB[key] = bind(types._PartialStatement.prototype[key], null);
  }
}

QB.bool = function (value) {return new types.BooleanLiteral(value);};
QB.num = function (value) {return new types.NumberLiteral(value);};
QB.int_ = function (value) {return new types.IntegerLiteral(value);};
QB.str = function (value) {return new types.StringLiteral(value);};
QB.list = function (arr) {return new types.ListLiteral(arr);};
QB.obj = function (obj) {return new types.ObjectLiteral(obj);};
QB.range = function (start, end) {return new types.RangeExpression(start, end);};
QB.get = function (obj, key) {return new types.PropertyAccess(obj, key);};
QB.ref = function (value) {
  if (types.Identifier.re.exec(value)) {
    return new types.Identifier(value);
  }
  return new types.SimpleReference(value);
};
QB.expr = function (value) {return new types.RawExpression(value);};

QB.and = function (/* x, y... */) {return new types.NAryOperation('&&', toArray(arguments));};
QB.or = function (/* x, y... */) {return new types.NAryOperation('||', toArray(arguments));};
QB.add = function (/* x, y... */) {return new types.NAryOperation('+', toArray(arguments));};
QB.sub = function (/* x, y... */) {return new types.NAryOperation('-', toArray(arguments));};
QB.mul = function (/* x, y... */) {return new types.NAryOperation('*', toArray(arguments));};
QB.div = function (/* x, y... */) {return new types.NAryOperation('/', toArray(arguments));};
QB.mod = function (/* x, y... */) {return new types.NAryOperation('%', toArray(arguments));};
QB.eq = function (x, y) {return new types.BinaryOperation('==', x, y);};
QB.gt = function (x, y) {return new types.BinaryOperation('>', x, y);};
QB.gte = function (x, y) {return new types.BinaryOperation('>=', x, y);};
QB.lt = function (x, y) {return new types.BinaryOperation('<', x, y);};
QB.lte = function (x, y) {return new types.BinaryOperation('<=', x, y);};
QB.neq = function (x, y) {return new types.BinaryOperation('!=', x, y);};
QB.not = function (x) {return new types.UnaryOperation('!', x);};
QB.neg = function (x) {return new types.UnaryOperation('-', x);};
QB.in_ = function (x, y) {return new types.BinaryOperation('in', x, y);};
QB.if_ = function (x, y, z) {return new types.TernaryOperation('?', ':', x, y, z);};

QB.plus = QB.add;
QB.minus = QB.sub;
QB.times = QB.mul;

QB['int'] = QB.int_;
QB['in'] = QB.in_;
QB['if'] = QB.if_;

QB.fn = function (functionName, arity) {
  if (typeof arity === 'number') {
    arity = [arity];
  }
  return function () {
    var args = Array.prototype.slice.call(arguments), valid, i;
    if (arity) {
      valid = false;
      for (i = 0; !valid && i < arity.length; i++) {
        if (typeof arity[i] === 'number') {
          if (args.length === arity[i]) {
            valid = true;
          }
        } else if (
          Object.prototype.toString.call(arity[i]) === '[object Array]' &&
            args.length >= arity[i][0] && args.length <= arity[i][1]
        ) {
          valid = true;
        }
      }
      if (!valid) {
        throw new AqlError(
          'Invalid number of arguments for function ' +
            functionName + ': ' + args.length
        );
      }
    }
    return new types.FunctionCall(functionName, args);
  };
};

function deprecateAqlFunction(fn, functionName) {
  return function () {
    warn('The AQL function ' + functionName + ' is deprecated!');
    return fn.apply(this, arguments);
  };
}

for (var key in assumptions.builtins) {
  if (assumptions.builtins.hasOwnProperty(key)) {
    QB[key] = QB.fn(key, assumptions.builtins[key]);
    if (assumptions.deprecatedBuiltins.indexOf(key) !== -1) {
      QB[key] = deprecateAqlFunction(QB[key], key);
    }
  }
}

module.exports = QB;