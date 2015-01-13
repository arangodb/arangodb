/*jshint browserify: true */
'use strict';
var AqlError = require('./errors').AqlError;
var keywords = require('./assumptions').keywords;

function wrapAQL(expr) {
  if (
    expr instanceof Operation ||
    expr instanceof Statement ||
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
  if (str.charAt(0) === '"') {
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
    return new Identifier(obj.name());
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
  if (Object.keys(autoCastToken).indexOf(type) === -1) {
    throw new AqlError('Invalid AQL value: (' + type + ') ' + token);
  }
  return autoCastToken[type](token);
}
autoCastToken.number = castNumber;
autoCastToken.boolean = castBoolean;
autoCastToken.string = castString;
autoCastToken.object = castObject;

function Expression() {}
function Operation() {}
Operation.prototype = new Expression();
Operation.prototype.constructor = Operation;

function RawExpression(value) {
  if (value && value instanceof RawExpression) {value = value.value;}
  this.value = value;
}
RawExpression.prototype = new Expression();
RawExpression.prototype.constructor = RawExpression;
RawExpression.prototype.toAQL = function () {return String(this.value);};

function NullLiteral(value) {
  if (value && value instanceof NullLiteral) {value = value.value;}
  if (value !== null && value !== undefined) {
    throw new AqlError('Expected value to be null: ' + value);
  }
  this.value = value;
}
NullLiteral.prototype = new Expression();
NullLiteral.prototype.constructor = NullLiteral;
NullLiteral.prototype.toAQL = function () {return 'null';};

function BooleanLiteral(value) {
  if (value && value instanceof BooleanLiteral) {value = value.value;}
  this.value = Boolean(value);
}
BooleanLiteral.prototype = new Expression();
BooleanLiteral.prototype.constructor = BooleanLiteral;
BooleanLiteral.prototype.toAQL = function () {return String(this.value);};

function NumberLiteral(value) {
  if (value && (
    value instanceof NumberLiteral ||
    value instanceof IntegerLiteral
  )) {value = value.value;}
  this.value = Number(value);
  if (!isValidNumber(this.value)) {
    throw new AqlError('Expected value to be a finite number: ' + value);
  }
}
NumberLiteral.re = /^[-+]?[0-9]+(\.[0-9]+)?$/;
NumberLiteral.prototype = new Expression();
NumberLiteral.prototype.constructor = NumberLiteral;
NumberLiteral.prototype.toAQL = function () {return String(this.value);};

function IntegerLiteral(value) {
  if (value && (
    value instanceof NumberLiteral ||
    value instanceof IntegerLiteral
  )) {value = value.value;}
  this.value = Number(value);
  if (!isValidNumber(this.value) || Math.floor(this.value) !== this.value) {
    throw new AqlError('Expected value to be a finite integer: ' + value);
  }
}
IntegerLiteral.prototype = new Expression();
IntegerLiteral.prototype.constructor = IntegerLiteral;
IntegerLiteral.prototype.toAQL = function () {return String(this.value);};

function StringLiteral(value) {
  if (value && value instanceof StringLiteral) {value = value.value;}
  if (value && typeof value.toAQL === 'function') {value = value.toAQL();}
  this.value = String(value);
}
StringLiteral.prototype = new Expression();
StringLiteral.prototype.constructor = StringLiteral;
StringLiteral.prototype.toAQL = function () {return JSON.stringify(this.value);};

function ListLiteral(value) {
  if (value && value instanceof ListLiteral) {value = value.value;}
  if (!value || !Array.isArray(value)) {
    throw new AqlError('Expected value to be an array: ' + value);
  }
  this.value = value.map(autoCastToken);
}
ListLiteral.prototype = new Expression();
ListLiteral.prototype.constructor = ListLiteral;
ListLiteral.prototype.toAQL = function () {
  var value = this.value.map(wrapAQL);
  return '[' + value.join(', ') + ']';
};

function ObjectLiteral(value) {
  if (value && value instanceof ObjectLiteral) {value = value.value;}
  if (!value || typeof value !== 'object') {
    throw new AqlError('Expected value to be an object: ' + value);
  }
  this.value = {};
  var self = this;
  Object.keys(value).forEach(function (key) {
    self.value[key] = autoCastToken(value[key]);
  });
}
ObjectLiteral.prototype = new Expression();
ObjectLiteral.prototype.constructor = ObjectLiteral;
ObjectLiteral.prototype.toAQL = function () {
  var value = this.value;
  var items = Object.keys(value).map(function (key) {
    if (key.match(Identifier.re) || key === String(Number(key))) {
      return key + ': ' + wrapAQL(value[key]);
    } else {
      return JSON.stringify(key) + ': ' + wrapAQL(value[key]);
    }
  });
  return '{' + items.join(', ') + '}';
};

function RangeExpression(start, end) {
  this.start = autoCastToken(start);
  this.end = autoCastToken(end);
}
RangeExpression.re = /^([0-9]+)\.\.([0-9]+)$/;
RangeExpression.prototype = new Expression();
RangeExpression.prototype.constructor = RangeExpression;
RangeExpression.prototype.toAQL = function () {
  return wrapAQL(this.start) + '..' + wrapAQL(this.end);
};

function PropertyAccess(obj, key) {
  this.obj = autoCastToken(obj);
  this.key = autoCastToken(key);
}
PropertyAccess.prototype = new Expression();
PropertyAccess.prototype.constructor = PropertyAccess;
PropertyAccess.prototype.toAQL = function () {
  return wrapAQL(this.obj) + '[' + wrapAQL(this.key) + ']';
};

function Keyword(value) {
  if (value && value instanceof Keyword) {value = value.value;}
  if (!value || typeof value !== 'string') {
    throw new AqlError('Expected value to be a string: ' + value);
  }
  if (!value.match(Keyword.re)) {
    throw new AqlError('Not a valid keyword: ' + value);
  }
  this.value = value;
}
Keyword.re = /^[_a-z][_0-9a-z]*$/i;
Keyword.prototype = new Expression();
Keyword.prototype.constructor = Keyword;
Keyword.prototype.toAQL = function () {
  return String(this.value).toUpperCase();
};

function Identifier(value) {
  if (value && value instanceof Identifier) {value = value.value;}
  if (!value || typeof value !== 'string') {
    throw new AqlError('Expected value to be a string: ' + value);
  }
  if (!value.match(Identifier.re)) {
    throw new AqlError('Not a valid identifier: ' + value);
  }
  this.value = value;
}
Identifier.re = /^[_a-z][-_0-9a-z]*$/i;
Identifier.prototype = new Expression();
Identifier.prototype.constructor = Identifier;
Identifier.prototype.toAQL = function () {
  var value = String(this.value);
  if (keywords.indexOf(value.toLowerCase()) === -1 && value.indexOf('-') === -1) {
    return value;
  }
  return '`' + value + '`';
};

function SimpleReference(value) {
  if (value && value instanceof SimpleReference) {value = value.value;}
  if (!value || typeof value !== 'string') {
    throw new AqlError('Expected value to be a string: ' + value);
  }
  if (!value.match(SimpleReference.re)) {
    throw new AqlError('Not a valid simple reference: ' + value);
  }
  this.value = value;
}
SimpleReference.re = /^@{0,2}[_a-z][_0-9a-z]*(\.[_a-z][_0-9a-z]*|\[\*\])*$/i;
SimpleReference.prototype = new Expression();
SimpleReference.prototype.constructor = SimpleReference;
SimpleReference.prototype.toAQL = function () {
  var value = String(this.value);
  var tokens = value.split('.').map(function (token) {
    return keywords.indexOf(token) === -1 ? token : '`' + token + '`';
  });
  return tokens.join('.');
};

function UnaryOperation(operator, value) {
  if (!operator || typeof operator !== 'string') {
    throw new AqlError('Expected operator to be a string: ' + operator);
  }
  this.operator = operator;
  this.value = autoCastToken(value);
}
UnaryOperation.prototype = new Expression();
UnaryOperation.prototype.constructor = UnaryOperation;
UnaryOperation.prototype.toAQL = function () {
  return this.operator + wrapAQL(this.value);
};

function BinaryOperation(operator, value1, value2) {
  if (!operator || typeof operator !== 'string') {
    throw new AqlError('Expected operator to be a string: ' + operator);
  }
  this.operator = operator;
  this.value1 = autoCastToken(value1);
  this.value2 = autoCastToken(value2);
}
BinaryOperation.prototype = new Operation();
BinaryOperation.prototype.constructor = BinaryOperation;
BinaryOperation.prototype.toAQL = function () {
  return [wrapAQL(this.value1), this.operator, wrapAQL(this.value2)].join(' ');
};

function TernaryOperation(operator1, operator2, value1, value2, value3) {
  if (!operator1 || typeof operator1 !== 'string') {
    throw new AqlError('Expected operator 1 to be a string: ' + operator1);
  }
  if (!operator2 || typeof operator2 !== 'string') {
    throw new AqlError('Expected operator 2 to be a string: ' + operator2);
  }
  this.operator1 = operator1;
  this.operator2 = operator2;
  this.value1 = autoCastToken(value1);
  this.value2 = autoCastToken(value2);
  this.value3 = autoCastToken(value3);
}
TernaryOperation.prototype = new Operation();
TernaryOperation.prototype.constructor = TernaryOperation;
TernaryOperation.prototype.toAQL = function () {
  return [
    wrapAQL(this.value1),
    this.operator1,
    wrapAQL(this.value2),
    this.operator2,
    wrapAQL(this.value3)
  ].join(' ');
};

function NAryOperation(operator, values) {
  if (!operator || typeof operator !== 'string') {
    throw new AqlError('Expected operator to be a string: ' + operator);
  }
  this.operator = operator;
  this.values = values.map(autoCastToken);
}
NAryOperation.prototype = new Operation();
NAryOperation.prototype.constructor = NAryOperation;
NAryOperation.prototype.toAQL = function () {
  var values = this.values.map(wrapAQL);
  return values.join(' ' + this.operator + ' ');
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
  this.functionName = functionName;
  this.args = args ? args.map(autoCastToken) : [];
}
FunctionCall.re = /^[_a-z][_0-9a-z]*(::[_a-z][_0-9a-z]*)*$/i;
FunctionCall.prototype = new Expression();
FunctionCall.prototype.constructor = FunctionCall;
FunctionCall.prototype.toAQL = function () {
  var args = this.args.map(wrapAQL);
  return this.functionName + '(' + args.join(', ') + ')';
};

function PartialStatement() {}
PartialStatement.prototype.for = function (varname) {
  var self = this, inFn;
  inFn = function (expr) {
    // assert expr is an expression
    return new ForExpression(self, varname, expr);
  };
  return {'in': inFn, in_: inFn};
};
PartialStatement.prototype.filter = function (expr) {return new FilterExpression(this, expr);};
PartialStatement.prototype.let = function (varname, expr) {
  if (expr === undefined) {
    return new LetExpression(this, varname);
  }
  return new LetExpression(this, [[varname, expr]]);
};
PartialStatement.prototype.collect = function (varname, expr) {
  if (expr === undefined) {
    return new CollectExpression(this, varname);
  }
  return new CollectExpression(this, [[varname, expr]]);
};
PartialStatement.prototype.sort = function () {
  var args = Array.prototype.slice.call(arguments);
  return new SortExpression(this, args);
};
PartialStatement.prototype.limit = function (x, y) {return new LimitExpression(this, x, y);};
PartialStatement.prototype.return = function (x) {return new ReturnExpression(this, x);};
PartialStatement.prototype.remove = function (expr) {
  var self = this, inFn;
  inFn = function (collection) {
    return new RemoveExpression(self, expr, collection);
  };
  return {into: inFn, 'in': inFn, in_: inFn};
};
PartialStatement.prototype.insert = function (expr) {
  var self = this, inFn;
  inFn = function (collection) {
    return new InsertExpression(self, expr, collection);
  };
  return {into: inFn, 'in': inFn, in_: inFn};
};
PartialStatement.prototype.update = function (expr) {
  var self = this, withFn, inFn;
  withFn = function (withExpr) {
    var inFn = function (collection) {
      return new UpdateExpression(self, expr, withExpr, collection);
    };
    return {into: inFn, 'in': inFn, in_: inFn};
  };
  inFn = function (collection) {
    return new ReplaceExpression(self, expr, undefined, collection);
  };
  return {'with': withFn, with_: withFn, into: inFn, 'in': inFn, in_: inFn};
};
PartialStatement.prototype.replace = function (expr) {
  var self = this, withFn, inFn;
  withFn = function (withExpr) {
    var inFn = function (collection) {
      return new ReplaceExpression(self, expr, withExpr, collection);
    };
    return {into: inFn, 'in': inFn, in_: inFn};
  };
  inFn = function (collection) {
    return new ReplaceExpression(self, expr, undefined, collection);
  };
  return {'with': withFn, with_: withFn, into: inFn, 'in': inFn, in_: inFn};
};

PartialStatement.prototype.for_ = PartialStatement.prototype.for;
PartialStatement.prototype.let_ = PartialStatement.prototype.let;
PartialStatement.prototype.return_ = PartialStatement.prototype.return;

function Definitions(dfns) {
  if (dfns instanceof Definitions) {
    dfns = dfns.dfns;
  }
  this.dfns = [];
  var self = this;
  if (!dfns || typeof dfns !== 'object') {
    throw new AqlError('Expected definitions to be an object');
  }
  if (Array.isArray(dfns)) {
    dfns.forEach(function (dfn, i) {
      if (!Array.isArray(dfn) || dfn.length !== 2) {
        throw new AqlError('Expected definitions[' + i + '] to be a tuple');
      }
      self.dfns.push([new Identifier(dfn[0]), autoCastToken(dfn[1])]);
    });
  } else {
    Object.keys(dfns).forEach(function (key) {
      self.dfns.push([new Identifier(key), autoCastToken(dfns[key])]);
    });
  }
  if (this.dfns.length === 0) {
    throw new AqlError('Expected definitions not to be empty');
  }
}
Definitions.prototype.toAQL = function () {
  return this.dfns.map(function (dfn) {
    return dfn[0].toAQL() + ' = ' + wrapAQL(dfn[1]);
  }).join(', ');
};

function ForExpression(prev, varname, expr) {
  this.prev = prev;
  this.varname = new Identifier(varname);
  this.expr = autoCastToken(expr);
}
ForExpression.prototype = new PartialStatement();
ForExpression.prototype.constructor = ForExpression;
ForExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'FOR ' + wrapAQL(this.varname) +
    ' IN ' + wrapAQL(this.expr)
  );
};

function FilterExpression(prev, expr) {
  this.prev = prev;
  this.expr = autoCastToken(expr);
}
FilterExpression.prototype = new PartialStatement();
FilterExpression.prototype.constructor = FilterExpression;
FilterExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'FILTER ' + wrapAQL(this.expr)
  );
};

function LetExpression(prev, dfns) {
  this.prev = prev;
  this.dfns = new Definitions(dfns);
}
LetExpression.prototype = new PartialStatement();
LetExpression.prototype.constructor = LetExpression;
LetExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'LET ' + this.dfns.toAQL()
  );
};

function CollectExpression(prev, dfns) {
  this.prev = prev;
  this.dfns = new Definitions(dfns);
}
CollectExpression.prototype = new PartialStatement();
CollectExpression.prototype.constructor = CollectExpression;
CollectExpression.prototype.into = function (varname) {
  return new CollectIntoExpression(this, varname);
};
CollectExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'COLLECT ' + this.dfns.toAQL()
  );
};
CollectExpression.prototype.keep = function () {
  var args = Array.prototype.slice.call(arguments);
  return new CollectKeepExpression(this, args);
};

function CollectIntoExpression(prev, varname) {
  this.prev = prev;
  this.varname = new Identifier(varname);
}
CollectIntoExpression.prototype = new PartialStatement();
CollectIntoExpression.prototype.constructor = CollectIntoExpression;
CollectIntoExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'INTO ' + this.varname.toAQL()
  );
};
CollectIntoExpression.prototype.count = function () {
  return new CollectCountExpression(this);
};
CollectIntoExpression.prototype.keep = CollectExpression.prototype.keep;

function CollectCountExpression(prev) {
  this.prev = prev;
}
CollectCountExpression.prototype = new PartialStatement();
CollectCountExpression.prototype.constructor = CollectCountExpression;
CollectCountExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'COUNT'
  );
};
CollectCountExpression.prototype.keep = CollectExpression.prototype.keep;

function CollectKeepExpression(prev, args) {
  if (!args || !Array.isArray(args)) {
    throw new AqlError('Expected sort list to be an array: ' + args);
  }
  if (!args.length) {
    throw new AqlError('Expected sort list not to be empty: ' + args);
  }
  this.prev = prev;
  this.args = args.map(autoCastToken);
}
CollectKeepExpression.prototype = new PartialStatement();
CollectKeepExpression.prototype.constructor = CollectKeepExpression;
CollectKeepExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'KEEP ' + this.args.map(wrapAQL).join(', ')
  );
};
CollectKeepExpression.prototype.keep = CollectExpression.prototype.keep;

function SortExpression(prev, args) {
  if (!args || !Array.isArray(args)) {
    throw new AqlError('Expected sort list to be an array: ' + args);
  }
  if (!args.length) {
    throw new AqlError('Expected sort list not to be empty: ' + args);
  }
  this.prev = prev;
  this.args = [];
  var allowKeyword = false;
  this.args = args.map(function (arg, i) {
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
  this.args.forEach(function (arg, i) {
    if (arg instanceof Keyword) {
      args[j] += ' ' + arg.toAQL();
    } else {
      j = args.push(wrapAQL(arg)) - 1;
    }
  });
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'SORT ' +
    args.join(', ')
  );
};

function LimitExpression(prev, offset, count) {
  if (count === undefined) {
    count = offset;
    offset = undefined;
  }
  this.prev = prev;
  this.offset = offset === undefined ? undefined : autoCastToken(offset);
  this.count = autoCastToken(count);
}
LimitExpression.prototype = new PartialStatement();
LimitExpression.prototype.constructor = LimitExpression;
LimitExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'LIMIT ' + (
      this.offset === undefined ?
      wrapAQL(this.count) :
      wrapAQL(this.offset) + ', ' + wrapAQL(this.count)
    )
  );
};

function Statement() {}
Statement.prototype = new Expression();
Statement.prototype.constructor = Statement;

function ReturnExpression(prev, value) {
  this.prev = prev;
  this.value = autoCastToken(value);
}
ReturnExpression.prototype = new Statement();
ReturnExpression.prototype.constructor = ReturnExpression;
ReturnExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'RETURN ' + wrapAQL(this.value)
  );
};

function RemoveExpression(prev, expr, collection) {
  this.prev = prev;
  this.expr = autoCastToken(expr);
  this.collection = new Identifier(collection);
}
RemoveExpression.prototype = new Statement();
RemoveExpression.prototype.constructor = RemoveExpression;
RemoveExpression.prototype.options = function (opts) {
  return new OptionsExpression(this, opts);
};
RemoveExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'REMOVE ' + wrapAQL(this.expr) +
    ' IN ' + wrapAQL(this.collection)
  );
};

function OptionsExpression(prev, opts) {
  this.prev = prev;
  this.opts = autoCastToken(opts);
}
OptionsExpression.prototype = new Statement();
OptionsExpression.prototype.constructor = OptionsExpression;
OptionsExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'OPTIONS ' + wrapAQL(this.opts)
  );
};

function InsertExpression(prev, expr, collection) {
  this.prev = prev;
  this.expr = autoCastToken(expr);
  this.collection = new Identifier(collection);
}
InsertExpression.prototype = new Statement();
InsertExpression.prototype.constructor = InsertExpression;
InsertExpression.prototype.options = function (opts) {
  return new OptionsExpression(this, opts);
};
InsertExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'INSERT ' + wrapAQL(this.expr) +
    ' INTO ' + wrapAQL(this.collection)
  );
};

function UpdateExpression(prev, expr, withExpr, collection) {
  this.prev = prev;
  this.expr = autoCastToken(expr);
  this.withExpr = withExpr === undefined ? undefined : autoCastToken(withExpr);
  this.collection = new Identifier(collection);
}
UpdateExpression.prototype = new Statement();
UpdateExpression.prototype.constructor = UpdateExpression;
UpdateExpression.prototype.options = function (opts) {
  return new OptionsExpression(this, opts);
};
UpdateExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'UPDATE ' + wrapAQL(this.expr) +
    (this.withExpr ? ' WITH ' + wrapAQL(this.withExpr) : '') +
    ' IN ' + wrapAQL(this.collection)
  );
};

function ReplaceExpression(prev, expr, withExpr, collection) {
  this.prev = prev;
  this.expr = autoCastToken(expr);
  this.withExpr = withExpr === undefined ? undefined : autoCastToken(withExpr);
  this.collection = new Identifier(collection);
}
ReplaceExpression.prototype = new Statement();
ReplaceExpression.prototype.constructor = ReplaceExpression;
ReplaceExpression.prototype.options = function (opts) {
  return new OptionsExpression(this, opts);
};
ReplaceExpression.prototype.toAQL = function () {
  return (
    (this.prev ? this.prev.toAQL() + ' ' : '') +
    'REPLACE ' + wrapAQL(this.expr) +
    (this.withExpr ? ' WITH ' + wrapAQL(this.withExpr) : '') +
    ' IN ' + wrapAQL(this.collection)
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
exports.SortExpression = SortExpression;
exports.LimitExpression = LimitExpression;
exports.ReturnExpression = ReturnExpression;
exports.RemoveExpression = RemoveExpression;
exports.InsertExpression = InsertExpression;
exports.UpdateExpression = UpdateExpression;
exports.ReplaceExpression = ReplaceExpression;

exports._Expression = Expression;
exports._Operation = Operation;
exports._Statement = Statement;
exports._PartialStatement = PartialStatement;
exports._Definitions = Definitions;
exports._CollectIntoExpression = CollectIntoExpression;
exports._CollectCountExpression = CollectCountExpression;
exports._CollectKeepExpression = CollectKeepExpression;
exports._OptionsExpression = OptionsExpression;
