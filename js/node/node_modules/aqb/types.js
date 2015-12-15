/*jshint browserify: true */
'use strict';
var AqlError = require('./errors').AqlError;
var keywords = require('./assumptions').keywords;

function toArray(self, args) {
  var arr = Array.prototype.slice.call(args);
  if (self) arr.unshift(self);
  return arr;
}

function isQuotedString(str) {
  return (
    str.length >= 2 &&
    str.charAt(0) === str.charAt(str.length - 1) &&
    str.charAt(0) === '"'
  );
}

function wrapAQL(expr) {
  if (
    expr instanceof Operation ||
    expr instanceof ReturnExpression ||
    expr instanceof PartialStatement
  ) {
    return '(' + expr.toAQL() + ')';
  }
  return expr.toAQL();
}

function isValidNumber(number) {
  return (
    number === number &&
    number !== Infinity &&
    number !== -Infinity
  );
}

function castNumber(number) {
  if (Math.floor(number) === number) {
    return new IntegerLiteral(number);
  }
  return new NumberLiteral(number);
}

function castBoolean(bool) {
  return new BooleanLiteral(bool);
}

function castString(str) {
  if (str.match(NumberLiteral.re)) {
    return autoCastToken(Number(str));
  }
  if (isQuotedString(str)) {
    return new StringLiteral(JSON.parse(str));
  }
  var match = str.match(RangeExpression.re);
  if (match) {
    return new RangeExpression(Number(match[1]), Number(match[2]));
  }
  if (str.match(Identifier.re)) {
    return new Identifier(str);
  }
  return new SimpleReference(str);
}

function castObject(obj) {
  if (obj.constructor && obj.constructor.name === 'ArangoCollection') {
    return new Identifier(obj);
  }
  if (Array.isArray(obj)) {
    return new ListLiteral(obj);
  }
  return new ObjectLiteral(obj);
}

function autoCastToken(token) {
  if (token === null || token === undefined) {
    return new NullLiteral();
  }
  if (token instanceof Expression || token instanceof PartialStatement) {
    return token;
  }
  var type = typeof token;
  if (!autoCastToken.hasOwnProperty(type)) {
    throw new AqlError('Invalid AQL value: (' + type + ') ' + token);
  }
  return autoCastToken[type](token);
}
autoCastToken.number = castNumber;
autoCastToken.boolean = castBoolean;
autoCastToken.string = castString;
autoCastToken.object = castObject;

function Definitions(dfns) {
  if (dfns instanceof Definitions) {
    dfns = dfns._dfns;
  }
  this._dfns = [];
  var self = this;
  if (!dfns || typeof dfns !== 'object') {
    throw new AqlError('Expected definitions to be an object');
  }
  if (Array.isArray(dfns)) {
    dfns.forEach(function (dfn, i) {
      if (!Array.isArray(dfn) || dfn.length !== 2) {
        throw new AqlError('Expected definitions[' + i + '] to be a tuple');
      }
      self._dfns.push([new Identifier(dfn[0]), autoCastToken(dfn[1])]);
    });
  } else {
    Object.keys(dfns).forEach(function (key) {
      self._dfns.push([new Identifier(key), autoCastToken(dfns[key])]);
    });
  }
  if (this._dfns.length === 0) {
    throw new AqlError('Expected definitions not to be empty');
  }
}
Definitions.prototype.toAQL = function () {
  return this._dfns.map(function (dfn) {
    return dfn[0].toAQL() + ' = ' + wrapAQL(dfn[1]);
  }).join(', ');
};

function Expression() {}
Expression.prototype.range = Expression.prototype.to = function (max) {
  return new RangeExpression(this, max);
};
Expression.prototype.get = function (/* keys... */) {
  return new PropertyAccess(this, toArray(null, arguments));
};
Expression.prototype.and = function (/* x, y... */) {
  return new NAryOperation('&&', toArray(this, arguments));
};
Expression.prototype.or = function (/* x, y... */) {
  return new NAryOperation('||', toArray(this, arguments));
};
Expression.prototype.add = Expression.prototype.plus = function (/* x, y... */) {
  return new NAryOperation('+', toArray(this, arguments));
};
Expression.prototype.sub = Expression.prototype.minus = function (/* x, y... */) {
  return new NAryOperation('-', toArray(this, arguments));
};
Expression.prototype.mul = Expression.prototype.times = function (/* x, y... */) {
  return new NAryOperation('*', toArray(this, arguments));
};
Expression.prototype.div = function (/* x, y... */) {
  return new NAryOperation('/', toArray(this, arguments));
};
Expression.prototype.mod = function (/* x, y... */) {
  return new NAryOperation('%', toArray(this, arguments));
};
Expression.prototype.eq = function (x) {
  return new BinaryOperation('==', this, x);
};
Expression.prototype.gt = function (x) {
  return new BinaryOperation('>', this, x);
};
Expression.prototype.gte = function (x) {
  return new BinaryOperation('>=', this, x);
};
Expression.prototype.lt = function (x) {
  return new BinaryOperation('<', this, x);
};
Expression.prototype.lte = function (x) {
  return new BinaryOperation('<=', this, x);
};
Expression.prototype.neq = function (x) {
  return new BinaryOperation('!=', this, x);
};
Expression.prototype.not = function () {
  return new UnaryOperation('!', this);
};
Expression.prototype.neg = function () {
  return new UnaryOperation('-', this);
};
Expression.prototype.in = function (x) {
  return new BinaryOperation('in', this, x);
};
Expression.prototype.notIn = function (x) {
  return new BinaryOperation('not in', this, x);
};
Expression.prototype.then = function (x) {
  var self = this;
  var elseFn = function (y) {
    return new TernaryOperation('?', ':', self, x, y);
  };
  return {else: elseFn, else_: elseFn, otherwise: elseFn};
};


function Operation() {}
Operation.prototype = new Expression();
Operation.prototype.constructor = Operation;

function RawExpression(value) {
  if (value && value instanceof RawExpression) value = value._value;
  this._value = value;
}
RawExpression.prototype = new Expression();
RawExpression.prototype.constructor = RawExpression;
RawExpression.prototype.toAQL = function () {
  return String(this._value);
};

function NullLiteral(value) {
  if (value && value instanceof NullLiteral) value = value._value;
  if (value !== null && value !== undefined) {
    throw new AqlError('Expected value to be null: ' + value);
  }
  this._value = value;
}
NullLiteral.prototype = new Expression();
NullLiteral.prototype.constructor = NullLiteral;
NullLiteral.prototype.toAQL = function () {
  return 'null';
};

function BooleanLiteral(value) {
  if (value && value instanceof BooleanLiteral) value = value._value;
  this._value = Boolean(value);
}
BooleanLiteral.prototype = new Expression();
BooleanLiteral.prototype.constructor = BooleanLiteral;
BooleanLiteral.prototype.toAQL = function () {
  return String(this._value);
};

function NumberLiteral(value) {
  if (value && (
    value instanceof NumberLiteral ||
    value instanceof IntegerLiteral
  )) value = value._value;
  this._value = Number(value);
  if (!isValidNumber(this._value)) {
    throw new AqlError('Expected value to be a finite number: ' + value);
  }
}
NumberLiteral.re = /^[-+]?[0-9]+(\.[0-9]+)?$/;
NumberLiteral.prototype = new Expression();
NumberLiteral.prototype.constructor = NumberLiteral;
NumberLiteral.prototype.toAQL = function () {
  return String(this._value);
};

function IntegerLiteral(value) {
  if (value && (
    value instanceof NumberLiteral ||
    value instanceof IntegerLiteral
  )) value = value._value;
  this._value = Number(value);
  if (!isValidNumber(this._value) || Math.floor(this._value) !== this._value) {
    throw new AqlError('Expected value to be a finite integer: ' + value);
  }
}
IntegerLiteral.prototype = new Expression();
IntegerLiteral.prototype.constructor = IntegerLiteral;
IntegerLiteral.prototype.toAQL = function () {
  return String(this._value);
};

function StringLiteral(value) {
  if (value && value instanceof StringLiteral) value = value._value;
  if (value && typeof value.toAQL === 'function') value = value.toAQL();
  this._value = String(value);
}
StringLiteral.prototype = new Expression();
StringLiteral.prototype.constructor = StringLiteral;
StringLiteral.prototype.toAQL = function () {
  return JSON.stringify(this._value);
};

function ListLiteral(value) {
  if (value && value instanceof ListLiteral) value = value._value;
  if (!value || !Array.isArray(value)) {
    throw new AqlError('Expected value to be an array: ' + value);
  }
  this._value = value.map(autoCastToken);
}
ListLiteral.prototype = new Expression();
ListLiteral.prototype.constructor = ListLiteral;
ListLiteral.prototype.toAQL = function () {
  var value = this._value.map(wrapAQL);
  return '[' + value.join(', ') + ']';
};

function ObjectLiteral(value) {
  if (value && value instanceof ObjectLiteral) value = value._value;
  if (!value || typeof value !== 'object') {
    throw new AqlError('Expected value to be an object: ' + value);
  }
  this._value = {};
  var self = this;
  Object.keys(value).forEach(function (key) {
    if (key.charAt(0) === ':') {
      if (!key.slice(1).match(SimpleReference.re)) {
        throw new AqlError('Expected key to be a well-formed dynamic property name: ' + key);
      }
      self._value['[' + key.slice(1) + ']'] = autoCastToken(value[key]);
    } else if (
      !isQuotedString(key) &&
      !key.match(Identifier.re) &&
      key !== String(Number(key))
    ) {
      self._value[JSON.stringify(key)] = autoCastToken(value[key]);
    } else {
      self._value[key] = autoCastToken(value[key]);
    }
  });
}
ObjectLiteral.prototype = new Expression();
ObjectLiteral.prototype.constructor = ObjectLiteral;
ObjectLiteral.prototype.toAQL = function () {
  var value = this._value;
  var items = Object.keys(value).map(function (key) {
    return key + ': ' + wrapAQL(value[key]);
  });
  return '{' + items.join(', ') + '}';
};

function RangeExpression(start, end) {
  this._start = autoCastToken(start);
  this._end = autoCastToken(end);
}
RangeExpression.re = /^([0-9]+)\.\.([0-9]+)$/;
RangeExpression.prototype = new Expression();
RangeExpression.prototype.constructor = RangeExpression;
RangeExpression.prototype.toAQL = function () {
  return wrapAQL(this._start) + '..' + wrapAQL(this._end);
};

function PropertyAccess(obj, keys) {
  this._obj = autoCastToken(obj);
  this._keys = keys.map(function (key) {
    return autoCastToken(key);
  });
}
PropertyAccess.prototype = new Expression();
PropertyAccess.prototype.constructor = PropertyAccess;
PropertyAccess.prototype.toAQL = function () {
  return wrapAQL(this._obj) + this._keys.map(function (key) {
    return '[' + wrapAQL(key) + ']';
  }).join('');
};

function Keyword(value) {
  if (value && value instanceof Keyword) value = value._value;
  if (!value || typeof value !== 'string') {
    throw new AqlError('Expected value to be a string: ' + value);
  }
  if (!value.match(Keyword.re)) {
    throw new AqlError('Not a valid keyword: ' + value);
  }
  this._value = value;
}
Keyword.re = /^[_a-z][_0-9a-z]*$/i;
Keyword.prototype = new Expression();
Keyword.prototype.constructor = Keyword;
Keyword.prototype.toAQL = function () {
  return String(this._value).toUpperCase();
};

function Identifier(value) {
  if (value) {
    if (value.constructor && value.constructor.name === 'ArangoCollection') {
      value = value.name();
    } else if (value instanceof Identifier) {
      value = value._value;
    }
  }
  if (!value || typeof value !== 'string') {
    throw new AqlError('Expected value to be a string: ' + value);
  }
  if (!value.match(Identifier.re)) {
    throw new AqlError('Not a valid identifier: ' + value);
  }
  this._value = value;
}
Identifier.re = /^[_@a-z][-_@0-9a-z]*$/i;
Identifier.prototype = new Expression();
Identifier.prototype.constructor = Identifier;
Identifier.prototype.toAQL = function () {
  var value = String(this._value);
  if (keywords.indexOf(value.toLowerCase()) !== -1 || value.indexOf('-') !== -1) {
    return '`' + value + '`';
  }
  return value;
};

function SimpleReference(value) {
  if (value) {
    if (value.constructor && value.constructor.name === 'ArangoCollection') {
      value = value.name();
    } else if (value instanceof SimpleReference) {
      value = value._value;
    }
  }
  if (!value || typeof value !== 'string') {
    throw new AqlError('Expected value to be a string: ' + value);
  }
  if (!value.match(SimpleReference.re)) {
    throw new AqlError('Not a valid simple reference: ' + value);
  }
  this._value = value;
}
SimpleReference.re = /^([_@a-z][-_@0-9a-z]*|`[_@a-z][-_@0-9a-z]*`)(\.[_@a-z][-_@0-9a-z]*|\.`[_@a-z][-_@0-9a-z]*`|\[\*\])*$/i;
SimpleReference.prototype = new Expression();
SimpleReference.prototype.constructor = SimpleReference;
SimpleReference.prototype.toAQL = function () {
  var value = String(this._value);
  var tokens = value.split('.').map(function (token) {
    if (token.charAt(0) !== '`' && (keywords.indexOf(token.toLowerCase()) !== -1 || token.indexOf('-') !== -1)) {
      return '`' + token + '`';
    }
    return token;
  });
  return tokens.join('.');
};

function UnaryOperation(operator, value) {
  if (!operator || typeof operator !== 'string') {
    throw new AqlError('Expected operator to be a string: ' + operator);
  }
  this._operator = operator;
  this._value = autoCastToken(value);
}
UnaryOperation.prototype = new Expression();
UnaryOperation.prototype.constructor = UnaryOperation;
UnaryOperation.prototype.toAQL = function () {
  return this._operator + wrapAQL(this._value);
};

function BinaryOperation(operator, value1, value2) {
  if (!operator || typeof operator !== 'string') {
    throw new AqlError('Expected operator to be a string: ' + operator);
  }
  this._operator = operator;
  this._value1 = autoCastToken(value1);
  this._value2 = autoCastToken(value2);
}
BinaryOperation.prototype = new Operation();
BinaryOperation.prototype.constructor = BinaryOperation;
BinaryOperation.prototype.toAQL = function () {
  return [wrapAQL(this._value1), this._operator, wrapAQL(this._value2)].join(' ');
};

function TernaryOperation(operator1, operator2, value1, value2, value3) {
  if (!operator1 || typeof operator1 !== 'string') {
    throw new AqlError('Expected operator 1 to be a string: ' + operator1);
  }
  if (!operator2 || typeof operator2 !== 'string') {
    throw new AqlError('Expected operator 2 to be a string: ' + operator2);
  }
  this._operator1 = operator1;
  this._operator2 = operator2;
  this._value1 = autoCastToken(value1);
  this._value2 = autoCastToken(value2);
  this._value3 = autoCastToken(value3);
}
TernaryOperation.prototype = new Operation();
TernaryOperation.prototype.constructor = TernaryOperation;
TernaryOperation.prototype.toAQL = function () {
  return [
    wrapAQL(this._value1),
    this._operator1,
    wrapAQL(this._value2),
    this._operator2,
    wrapAQL(this._value3)
  ].join(' ');
};

function NAryOperation(operator, values) {
  if (!operator || typeof operator !== 'string') {
    throw new AqlError('Expected operator to be a string: ' + operator);
  }
  this._operator = operator;
  this._values = values.map(autoCastToken);
}
NAryOperation.prototype = new Operation();
NAryOperation.prototype.constructor = NAryOperation;
NAryOperation.prototype.toAQL = function () {
  var values = this._values.map(wrapAQL);
  return values.join(' ' + this._operator + ' ');
};

function FunctionCall(functionName, args) {
  if (!functionName || typeof functionName !== 'string') {
    throw new AqlError('Expected function name to be a string: ' + functionName);
  }
  if (!functionName.match(FunctionCall.re)) {
    throw new AqlError('Not a valid function name: ' + functionName);
  }
  if (args && !Array.isArray(args)) {
    throw new AqlError('Expected arguments to be an array: ' + args);
  }
  this._functionName = functionName;
  this._args = args ? args.map(autoCastToken) : [];
}
FunctionCall.re = /^[_a-z][_0-9a-z]*(::[_a-z][_0-9a-z]*)*$/i;
FunctionCall.prototype = new Expression();
FunctionCall.prototype.constructor = FunctionCall;
FunctionCall.prototype.toAQL = function () {
  var args = this._args.map(wrapAQL);
  return this._functionName + '(' + args.join(', ') + ')';
};

function ReturnExpression(prev, value, distinct) {
  this._prev = prev;
  this._value = autoCastToken(value);
  this._distinct = distinct;
}
ReturnExpression.prototype = new Expression();
ReturnExpression.prototype.constructor = ReturnExpression;
ReturnExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'RETURN' +
    (this._distinct ? ' DISTINCT' : '') +
    ' ' + wrapAQL(this._value)
  );
};

function PartialStatement() {}
PartialStatement.prototype.for = function (varname) {
  var self = this, inFn;
  inFn = function (expr) {
    // assert expr is an expression
    return new ForExpression(self, varname, expr);
  };
  return {in: inFn, in_: inFn};
};
PartialStatement.prototype.filter = function (expr) {
  return new FilterExpression(this, expr);
};
PartialStatement.prototype.let = function (varname, expr) {
  var dfns = expr === undefined ? varname : [[varname, expr]];
  return new LetExpression(this, dfns);
};
PartialStatement.prototype.collect = function (varname, expr) {
  var dfns = expr === undefined ? varname : [[varname, expr]];
  return new CollectExpression(this, dfns);
};
PartialStatement.prototype.collectWithCountInto = function (varname) {
  return new CollectWithCountIntoExpression(this, undefined, varname);
};
PartialStatement.prototype.sort = function () {
  var args = Array.prototype.slice.call(arguments);
  return new SortExpression(this, args);
};
PartialStatement.prototype.limit = function (x, y) {
  return new LimitExpression(this, x, y);
};
PartialStatement.prototype.return = function (x) {
  return new ReturnExpression(this, x, false);
};
PartialStatement.prototype.returnDistinct = function (x) {
  return new ReturnExpression(this, x, true);
};
PartialStatement.prototype.remove = function (expr) {
  var self = this, inFn;
  inFn = function (collection) {
    return new RemoveExpression(self, expr, collection);
  };
  return {into: inFn, in: inFn, in_: inFn};
};
PartialStatement.prototype.upsert = function (upsertExpr) {
  var self = this;
  function insertFn(insertExpr) {
    function updateOrReplaceFn(replace) {
      return function (updateOrReplaceExpr) {
        function inFn(inCollection) {
          return new UpsertExpression(
            self,
            upsertExpr,
            insertExpr,
            replace,
            updateOrReplaceExpr,
            inCollection
          );
        }
        return {into: inFn, in: inFn, in_: inFn};
      };
    }
    return {
      update: updateOrReplaceFn(false),
      replace: updateOrReplaceFn(true)
    };
  }
  return {insert: insertFn};
};
PartialStatement.prototype.insert = function (expr) {
  var self = this, inFn;
  inFn = function (collection) {
    return new InsertExpression(self, expr, collection);
  };
  return {into: inFn, in: inFn, in_: inFn};
};
PartialStatement.prototype.update = function (expr) {
  var self = this, withFn, inFn;
  withFn = function (withExpr) {
    var inFn = function (collection) {
      return new UpdateExpression(self, expr, withExpr, collection);
    };
    return {into: inFn, in: inFn, in_: inFn};
  };
  inFn = function (collection) {
    return new UpdateExpression(self, expr, undefined, collection);
  };
  return {'with': withFn, with_: withFn, into: inFn, in: inFn, in_: inFn};
};
PartialStatement.prototype.replace = function (expr) {
  var self = this, withFn, inFn;
  withFn = function (withExpr) {
    var inFn = function (collection) {
      return new ReplaceExpression(self, expr, withExpr, collection);
    };
    return {into: inFn, in: inFn, in_: inFn};
  };
  inFn = function (collection) {
    return new ReplaceExpression(self, expr, undefined, collection);
  };
  return {'with': withFn, with_: withFn, into: inFn, in: inFn, in_: inFn};
};

function ForExpression(prev, varname, expr) {
  this._prev = prev;
  this._varname = new Identifier(varname);
  this._expr = autoCastToken(expr);
}
ForExpression.prototype = new PartialStatement();
ForExpression.prototype.constructor = ForExpression;
ForExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'FOR ' + wrapAQL(this._varname) +
    ' IN ' + wrapAQL(this._expr)
  );
};

function FilterExpression(prev, expr) {
  this._prev = prev;
  this._expr = autoCastToken(expr);
}
FilterExpression.prototype = new PartialStatement();
FilterExpression.prototype.constructor = FilterExpression;
FilterExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'FILTER ' + wrapAQL(this._expr)
  );
};

function LetExpression(prev, dfns) {
  this._prev = prev;
  this._dfns = new Definitions(dfns);
}
LetExpression.prototype = new PartialStatement();
LetExpression.prototype.constructor = LetExpression;
LetExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'LET ' + wrapAQL(this._dfns)
  );
};

function CollectExpression(prev, dfns, varname, intoExpr, keepNames, options) {
  this._prev = prev;
  this._dfns = new Definitions(dfns);
  if (!intoExpr) {
    if (!varname) {
      this.into = function (newVarname, newIntoExpr) {
        return new CollectExpression(prev, dfns, newVarname, newIntoExpr, undefined, options);
      };
    } else {
      this._varname = new Identifier(varname);
      if (!keepNames) {
        this.keep = function () {
          var newKeepNames = Array.prototype.slice.call(arguments);
          return new CollectExpression(prev, dfns, varname, undefined, newKeepNames, options);
        };
      } else {
        if (!Array.isArray(keepNames)) {
          throw new AqlError('Expected keep list to be an array: ' + keepNames);
        }
        if (!keepNames.length) {
          throw new AqlError('Expected keep list not to be empty: ' + keepNames);
        }
        this._keep = keepNames.map(function (keepVar) {
          return new Identifier(keepVar);
        });
      }
    }
  } else if (varname) {
    this._varname = new Identifier(varname);
    this._intoExpr = autoCastToken(intoExpr);
  }
  if (!options) {
    this.options = function (newOpts) {
      return new CollectExpression(prev, dfns, varname, intoExpr, keepNames, newOpts);
    };
  } else this._options = new ObjectLiteral(options);
  if (!varname && !intoExpr && !keepNames) {
    this.withCountInto = function (newVarname) {
      return new CollectWithCountIntoExpression(prev, dfns, newVarname, options);
    };
  }
}
CollectExpression.prototype = new PartialStatement();
CollectExpression.prototype.constructor = CollectExpression;
CollectExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'COLLECT ' + wrapAQL(this._dfns) +
    (this._varname ? ' INTO ' + wrapAQL(this._varname) + (
      this._intoExpr ? ' = ' + wrapAQL(this._intoExpr) :
      (this._keep ? ' KEEP ' + this._keep.map(function (keep) {
        return keep.toAQL();
      }).join(', ') : '')
    ) : '') +
    (this._options ? ' OPTIONS ' + wrapAQL(this._options) : '')
  );
};

function CollectWithCountIntoExpression(prev, dfns, varname, options) {
  this._prev = prev;
  if (dfns) this._dfns = new Definitions(dfns);
  this._varname = new Identifier(varname);
  if (!options) {
    this.options = function (newOpts) {
      return new CollectWithCountIntoExpression(prev, dfns, varname, newOpts);
    };
  } else this._options = new ObjectLiteral(options);
}
CollectWithCountIntoExpression.prototype = new PartialStatement();
CollectWithCountIntoExpression.prototype.constructor = CollectWithCountIntoExpression;
CollectWithCountIntoExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'COLLECT' +
    (this._dfns ? ' ' + wrapAQL(this._dfns) : '') +
    ' WITH COUNT INTO ' + wrapAQL(this._varname) +
    (this._options ? ' OPTIONS ' + wrapAQL(this._options) : '')
  );
};

function SortExpression(prev, args) {
  if (!args || !Array.isArray(args)) {
    throw new AqlError('Expected sort list to be an array: ' + args);
  }
  if (!args.length) {
    throw new AqlError('Expected sort list not to be empty: ' + args);
  }
  this._prev = prev;
  this._args = [];
  var allowKeyword = false;
  this._args = args.map(function (arg, i) {
    if (!allowKeyword && arg) {
      if (arg instanceof Keyword || (
        typeof arg === 'string' && SortExpression.keywords.indexOf(arg.toUpperCase()) !== -1
      )) {
        throw new AqlError('Unexpected keyword ' + arg.toString() + ' at offset ' + i);
      }
    }
    if (typeof arg === 'string' && SortExpression.keywords.indexOf(arg.toUpperCase()) !== -1) {
      allowKeyword = false;
      return new Keyword(arg);
    } else {
      allowKeyword = true;
      return autoCastToken(arg);
    }
  });
}
SortExpression.keywords = ['ASC', 'DESC'];
SortExpression.prototype = new PartialStatement();
SortExpression.prototype.constructor = SortExpression;
SortExpression.prototype.toAQL = function () {
  var args = [], j = 0;
  this._args.forEach(function (arg) {
    if (arg instanceof Keyword) {
      args[j] += ' ' + arg.toAQL();
    } else {
      j = args.push(wrapAQL(arg)) - 1;
    }
  });
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'SORT ' +
    args.join(', ')
  );
};

function LimitExpression(prev, offset, count) {
  if (count === undefined) {
    count = offset;
    offset = undefined;
  }
  this._prev = prev;
  this._offset = offset === undefined ? undefined : autoCastToken(offset);
  this._count = autoCastToken(count);
}
LimitExpression.prototype = new PartialStatement();
LimitExpression.prototype.constructor = LimitExpression;
LimitExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'LIMIT ' + (
      this._offset === undefined ?
      wrapAQL(this._count) :
      wrapAQL(this._offset) + ', ' + wrapAQL(this._count)
    )
  );
};

function RemoveExpression(prev, expr, collection, options) {
  this._prev = prev;
  this._expr = autoCastToken(expr);
  this._collection = new Identifier(collection);
  if (!options) {
    this.options = function (newOpts) {
      return new RemoveExpression(prev, expr, collection, newOpts);
    };
  } else this._options = new ObjectLiteral(options);
}
RemoveExpression.prototype = new PartialStatement();
RemoveExpression.prototype.constructor = RemoveExpression;
RemoveExpression.prototype.returnOld = function (x) {
  return this.let(x, 'OLD').return(x);
};
RemoveExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'REMOVE ' + wrapAQL(this._expr) +
    ' IN ' + wrapAQL(this._collection) +
    (this._options ? ' OPTIONS ' + wrapAQL(this._options) : '')
  );
};

function UpsertExpression(prev, upsertExpr, insertExpr, replace, updateOrReplaceExpr, collection, options) {
  this._prev = prev;
  this._upsertExpr = autoCastToken(upsertExpr);
  this._insertExpr = autoCastToken(insertExpr);
  this._updateOrReplace = replace ? 'REPLACE' : 'UPDATE';
  this._updateOrReplaceExpr = autoCastToken(updateOrReplaceExpr);
  this._collection = new Identifier(collection);
  if (!options) {
    this.options = function (newOpts) {
      return new UpsertExpression(
        prev,
        upsertExpr,
        insertExpr,
        replace,
        updateOrReplaceExpr,
        collection,
        newOpts
      );
    };
  } else this._options = new ObjectLiteral(options);
}
UpsertExpression.prototype = new PartialStatement();
UpsertExpression.prototype.constructor = UpsertExpression;
UpsertExpression.prototype.returnNew = function (x) {
  return this.let(x, 'NEW').return(x);
};
UpsertExpression.prototype.returnOld = function (x) {
  return this.let(x, 'OLD').return(x);
};
UpsertExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'UPSERT ' + wrapAQL(this._upsertExpr) +
    ' INSERT ' + wrapAQL(this._insertExpr) +
    ' ' + this._updateOrReplace + ' ' + wrapAQL(this._updateOrReplaceExpr) +
    ' IN ' + wrapAQL(this._collection) +
    (this._options ? ' OPTIONS ' + wrapAQL(this._options) : '')
  );
};

function InsertExpression(prev, expr, collection, options) {
  this._prev = prev;
  this._expr = autoCastToken(expr);
  this._collection = new Identifier(collection);
  if (!options) {
    this.options = function (newOpts) {
      return new InsertExpression(prev, expr, collection, newOpts);
    };
  } else this._options = new ObjectLiteral(options);
}
InsertExpression.prototype = new PartialStatement();
InsertExpression.prototype.constructor = InsertExpression;
InsertExpression.prototype.returnNew = function (x) {
  return this.let(x, 'NEW').return(x);
};
InsertExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'INSERT ' + wrapAQL(this._expr) +
    ' INTO ' + wrapAQL(this._collection) +
    (this._options ? ' OPTIONS ' + wrapAQL(this._options) : '')
  );
};

function UpdateExpression(prev, expr, withExpr, collection, options) {
  this._prev = prev;
  this._expr = autoCastToken(expr);
  this._withExpr = withExpr === undefined ? undefined : autoCastToken(withExpr);
  this._collection = new Identifier(collection);
  if (!options) {
    this.options = function (newOpts) {
      return new UpdateExpression(prev, expr, withExpr, collection, newOpts);
    };
  } else this._options = new ObjectLiteral(options);
}
UpdateExpression.prototype = new PartialStatement();
UpdateExpression.prototype.constructor = UpdateExpression;
UpdateExpression.prototype.returnNew = function (x) {
  return this.let(x, 'NEW').return(x);
};
UpdateExpression.prototype.returnOld = function (x) {
  return this.let(x, 'OLD').return(x);
};
UpdateExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'UPDATE ' + wrapAQL(this._expr) +
    (this._withExpr ? ' WITH ' + wrapAQL(this._withExpr) : '') +
    ' IN ' + wrapAQL(this._collection) +
    (this._options ? ' OPTIONS ' + wrapAQL(this._options) : '')
  );
};

function ReplaceExpression(prev, expr, withExpr, collection, options) {
  this._prev = prev;
  this._expr = autoCastToken(expr);
  this._withExpr = withExpr === undefined ? undefined : autoCastToken(withExpr);
  this._collection = new Identifier(collection);
  if (!options) {
    this.options = function (newOpts) {
      return new ReplaceExpression(prev, expr, withExpr, collection, newOpts);
    };
  } else this._options = new ObjectLiteral(options);
}
ReplaceExpression.prototype = new PartialStatement();
ReplaceExpression.prototype.constructor = ReplaceExpression;
ReplaceExpression.prototype.returnNew = function (x) {
  return this.let(x, 'NEW').return(x);
};
ReplaceExpression.prototype.returnOld = function (x) {
  return this.let(x, 'OLD').return(x);
};
ReplaceExpression.prototype.toAQL = function () {
  return (
    (this._prev ? this._prev.toAQL() + ' ' : '') +
    'REPLACE ' + wrapAQL(this._expr) +
    (this._withExpr ? ' WITH ' + wrapAQL(this._withExpr) : '') +
    ' IN ' + wrapAQL(this._collection) +
    (this._options ? ' OPTIONS ' + wrapAQL(this._options) : '')
  );
};

exports.autoCastToken = autoCastToken;
exports.RawExpression = RawExpression;
exports.NullLiteral = NullLiteral;
exports.BooleanLiteral = BooleanLiteral;
exports.NumberLiteral = NumberLiteral;
exports.IntegerLiteral = IntegerLiteral;
exports.StringLiteral = StringLiteral;
exports.ListLiteral = ListLiteral;
exports.ObjectLiteral = ObjectLiteral;
exports.RangeExpression = RangeExpression;
exports.PropertyAccess = PropertyAccess;
exports.Keyword = Keyword;
exports.Identifier = Identifier;
exports.SimpleReference = SimpleReference;
exports.UnaryOperation = UnaryOperation;
exports.BinaryOperation = BinaryOperation;
exports.TernaryOperation = TernaryOperation;
exports.NAryOperation = NAryOperation;
exports.FunctionCall = FunctionCall;
exports.ForExpression = ForExpression;
exports.FilterExpression = FilterExpression;
exports.LetExpression = LetExpression;
exports.CollectExpression = CollectExpression;
exports.CollectWithCountIntoExpression = CollectWithCountIntoExpression;
exports.SortExpression = SortExpression;
exports.LimitExpression = LimitExpression;
exports.ReturnExpression = ReturnExpression;
exports.RemoveExpression = RemoveExpression;
exports.UpsertExpression = UpsertExpression;
exports.InsertExpression = InsertExpression;
exports.UpdateExpression = UpdateExpression;
exports.ReplaceExpression = ReplaceExpression;

exports._Expression = Expression;
exports._Operation = Operation;
exports._Statement = ReturnExpression;
exports._PartialStatement = PartialStatement;
exports._Definitions = Definitions;
