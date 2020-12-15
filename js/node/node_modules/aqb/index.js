/*jshint browserify: true */
/*globals console: false */
'use strict';
var console = require('console');
var AqlError = require('./errors').AqlError;
var assumptions = require('./assumptions');
var types = require('./types');

function QB(obj) {
  if (typeof obj === 'string') {
    return QB.str(obj);
  }
  if (obj === null || obj === undefined) {
    return new types.NullLiteral();
  }
  if (typeof obj === 'object') {
    if (obj instanceof Date) {
      return types.autoCastToken(JSON.stringify(obj));
    }
    if (obj instanceof Array) {
      return new types.ListLiteral(obj.map(QB));
    }
    var result = {};
    for (var key in obj) {
      if (obj.hasOwnProperty(key)) {
        result[JSON.stringify(key)] = QB(obj[key]);
      }
    }
    return new types.ObjectLiteral(result);
  }
  return types.autoCastToken(obj);
}

Object.keys(types._PartialStatement.prototype).forEach(function (key) {
  if (key === 'constructor') return;
  QB[key] = types._PartialStatement.prototype[key].bind(null);
});

Object.keys(types._Expression.prototype).forEach(function (key) {
  if (key === 'constructor' || key === 'then') return;
  QB[key] = function () {
    return types._Expression.prototype[key].apply(
      types.autoCastToken(arguments[0]),
      Array.prototype.slice.call(arguments, 1)
    );
  };
});

QB.if = function (cond, then, otherwise) {
  return types.autoCastToken(cond).then(then).else(otherwise);
};
QB.bool = function (value) {
  return new types.BooleanLiteral(value);
};
QB.num = function (value) {
  return new types.NumberLiteral(value);
};
QB.int = function (value) {
  return new types.IntegerLiteral(value);
};
QB.str = function (value) {
  return new types.StringLiteral(value);
};
QB.list = function (arr) {
  return new types.ListLiteral(arr);
};
QB.obj = function (obj) {
  return new types.ObjectLiteral(obj);
};
QB.ref = function (value) {
  if (types.Identifier.re.exec(value)) {
    return new types.Identifier(value);
  }
  return new types.SimpleReference(value);
};
QB.expr = function (value) {
  return new types.RawExpression(value);
};
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
    console.warn('The AQL function ' + functionName + ' is deprecated!');
    return fn.apply(this, arguments);
  };
}

Object.keys(assumptions.builtins).forEach(function (key) {
  QB[key] = QB.fn(key, assumptions.builtins[key]);
  if (assumptions.deprecatedBuiltins.indexOf(key) !== -1) {
    QB[key] = deprecateAqlFunction(QB[key], key);
  }
});

module.exports = QB;
