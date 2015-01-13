# ArangoDB Query Builder

The query builder allows constructing complex AQL queries with a pure JavaScript fluid API.

[![license - APACHE-2.0](https://img.shields.io/npm/l/aqb.svg)](http://opensource.org/licenses/APACHE-2.0) [![Dependencies](https://img.shields.io/david/arangodb/aqbjs.svg)](https://david-dm.org/arangodb/aqbjs)

[![NPM status](https://nodei.co/npm/aqb.png?downloads=true&stars=true)](https://npmjs.org/package/aqb)

[![Build status](https://img.shields.io/travis/arangodb/aqbjs.svg)](https://travis-ci.org/arangodb/aqbjs) [![Coverage Status](https://img.shields.io/coveralls/arangodb/aqbjs.svg)](https://coveralls.io/r/arangodb/aqbjs?branch=master) [![Codacy rating](https://img.shields.io/codacy/1d5a1be201024e0caaf0aa6bc990231e.svg)](https://www.codacy.com/public/me_4/aqbjs)

# Install

## With NPM

```sh
npm install aqb
```

## With Bower

```sh
bower install aqb
```

## ArangoDB

As of ArangoDB 1.3, a version of `aqb` comes pre-installed with ArangoDB.

```js
var qb = require('aqb');
```

If you want to use a more recent version of `aqb` in a Foxx app, you can add it to your NPM dependencies as usual.

## Browser

This CommonJS module is compatible with [browserify](http://browserify.org).

If you don't want to use browserify, you can simply use the AMD-compatible [browserify bundle](https://raw.githubusercontent.com/arangodb/aqbjs/master/dist/aqb.min.js) (~30 kB minified, ~6 kB gzipped).

If you want to use this module in non-ES5 browsers like Microsoft Internet Explorer 8 and earlier, you may need to include [es5-shim](https://www.npmjs.com/package/es5-shim) or a similar ES5 polyfill.

## From source

```sh
git clone https://github.com/arangodb/aqbjs.git
cd aqbjs
npm install
npm run dist
```

# Example

```js
// in arangosh
var db = require('org/arangodb').db;
var qb = require('aqb');
console.log(db._query(qb.for('x').in('1..5').return('x')).toArray()); // [1, 2, 3, 4, 5]
```

# API

## Auto-casting raw data

By default, the query builder will attempt to interpret raw strings as identifiers or references or other kinds of expressions. This may not always be what you want, especially when handling raw untrusted data.

As of version 1.8 you can now pass arbitrary data directly to the query builder itself and it will be translated to the equivalent AQL structure (e.g. strings will be strings, dates will be converted to JSON, arrays and objects will be translated recursively, and so on):

```js
var doc = {
    aString: "hello",
    aDate: new Date(),
    aNumber: 23,
    anArray: [1, 2, 3, "potato"]
};
db._query(qb.insert(qb(doc)).into('my_collection'));
```

## AQL Types

If raw JavaScript values are passed to AQL statements, they will be wrapped in a matching AQL type automatically.

JavaScript strings wrapped in quotation marks will be wrapped in AQL strings, all other JavaScript strings will be wrapped as simple references (see below) and throw an *AQLError* if they are not well-formed.

### Boolean

Wraps the given value as an AQL Boolean literal.

`qb.bool(value)`

If the value is truthy, it will be converted to the AQL Boolean *true*, otherwise it will be converted to the AQL Boolean *false*.

If the value is already an AQL Boolean, its own value will be wrapped instead.

### Number

Wraps the given value as an AQL Number literal.

`qb.num(value)`

If the value is not a JavaScript Number, it will be converted first.

If the value does not represent a finite number, an *AQLError* will be thrown.

If the value is already an AQL Number or AQL Integer, its own value will be wrapped instead.

### Integer

Wraps the given value as an AQL Integer literal.

`qb.int(value)`

If the value is not a JavaScript Number, it will be converted first.

If the value does not represent a finite integer, an *AQLError* will be thrown.

If the value is already an AQL Number or AQL Integer, its own value will be wrapped instead.

*Alias:* `qb.int_(value)`

### String

Wraps the given value as an AQL String literal.

`qb.str(value)`

If the value is not a JavaScript String, it will be converted first.

If the value is already an AQL String, its own value will be wrapped instead.

If the value is an object with a *toAQL* method, the result of calling that method will be wrapped instead.

### List

Wraps the given value as an AQL List (Array) literal.

`qb.list(value)`

If the value is not a JavaScript Array, an *AQLError* will be thrown.

If the value is already an AQL List, its own value will be wrapped instead.

Any list elements that are not already AQL values will be converted automatically.

### Object

Wraps the given value as an AQL Object literal.

`qb.obj(value)`

If the value is not a JavaScript Object, an *AQLError* will be thrown.

If the value is already an AQL List, its own value will be wrapped instead.

Any property values that are not already AQL values will be converted automatically.

### Simple Reference

Wraps a given value in an AQL Simple Reference.

`qb.ref(value)`

If the value is not a JavaScript string or not a well-formed simple reference, an *AQLError* will be thrown.

If the value is an *ArangoCollection*, its *name* property will be used instead.

If the value is already an AQL Simple Reference, its value is wrapped instead.

*Examples*

Valid values:

* `foo`
* `foo.bar`
* `foo[*].bar`
* `foo.bar.QUX`
* `_foo._bar._qux`
* `foo1.bar2`

Invalid values:

* `1foo`
* `föö`
* `foo bar`
* `foo-bar`
* `foo[bar]`

ArangoDB collection objects can be passed directly:

```js
var myUserCollection = applicationContext.collection('users');
var users = db._query(qb.for('u').in(myUserCollection).return('u')).toArray();
```

## AQL Expressions

### Range

Creates a range expression from the given values.

`qb.range(value1, value2)` -> `value1..value2`

If the values are not already AQL values, they will be converted automatically.

### Property Access

Creates a property access expression from the given values.

`qb.get(obj, key)` -> `obj[key]`

If the values are not already AQL values, they will be converted automatically.

### Raw Expression

Wraps a given value in a raw AQL expression.

`qb.expr(value)`

If the value is already an AQL Raw Expression, its value is wrapped instead.

**Warning:** Whenever possible, you should use one of the other methods or a combination thereof instead of using a raw expression. Raw expressions allow passing arbitrary strings into your AQL and thus will open you to AQL injection attacks if you are passing in untrusted user input.

## AQL Operations

### Boolean And

Creates an "and" operation from the given values.

`qb.and(a, b)` -> `(a && b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Examples*

`qb.and(a, b, c, d, e, f)` -> `(a && b && c && d && e && f)`

### Boolean Or

Creates an "or" operation from the given values.

`qb.or(a, b)` -> `(a || b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Examples*

`qb.or(a, b, c, d, e, f)` -> `(a || b || c || d || e || f)`

### Addition

Creates an addition operation from the given values.

`qb.add(a, b)` -> `(a + b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Alias:* `qb.plus(a, b)`

*Examples*

`qb.add(a, b, c, d, e, f)` -> `(a + b + c + d + e + f)`

### Subtraction

Creates a subtraction operation from the given values.

`qb.sub(a, b)` -> `(a - b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Alias:* `qb.minus(a, b)`

*Examples*

`qb.sub(a, b, c, d, e, f)` -> `(a - b - c - d - e - f)`

### Multiplication

Creates a multiplication operation from the given values.

`qb.mul(a, b)` -> `(a * b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Alias:* `qb.times(a, b)`

*Examples*

`qb.mul(a, b, c, d, e, f)` -> `(a * b * c * d * e * f)`

### Division

Creates a division operation from the given values.

`qb.div(a, b)` -> `(a / b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Examples*

`qb.div(a, b, c, d, e, f)` -> `(a / b / c / d / e / f)`

### Modulus

Creates a modulus operation from the given values.

`qb.mod(a, b)` -> `(a % b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Examples*

`qb.mod(a, b, c, d, e, f)` -> `(a % b % c % d % e % f)`

### Equality

Creates an equality comparison from the given values.

`qb.eq(a, b)` -> `(a == b)`

If the values are not already AQL values, they will be converted automatically.

### Inequality

Creates an inequality comparison from the given values.

`qb.neq(a, b)` -> `(a != b)`

If the values are not already AQL values, they will be converted automatically.

### Greater Than

Creates a greater-than comparison from the given values.

`qb.gt(a, b)` -> `(a > b)`

If the values are not already AQL values, they will be converted automatically.

### Greater Than Or Equal To

Creates a greater-than-or-equal-to comparison from the given values.

`qb.gte(a, b)` -> `(a >= b)`

If the values are not already AQL values, they will be converted automatically.

### Less Than

Creates a less-than comparison from the given values.

`qb.lt(a, b)` -> `(a < b)`

If the values are not already AQL values, they will be converted automatically.

### Less Than Or Equal To

Creates a less-than-or-equal-to comparison from the given values.

`qb.lte(a, b)` -> `(a <= b)`

If the values are not already AQL values, they will be converted automatically.

### Contains

Creates an "in" comparison from the given values.

`qb.in(a, b)` -> `(a in b)`

If the values are not already AQL values, they will be converted automatically.

*Aliases:* `qb.in_(a, b)`

### Negation

Creates a negation from the given value.

`qb.not(a)` -> `!(a)`

If the value is not already an AQL value, it will be converted automatically.

### Negative Value

Creates a negative value expression from the given value.

`qb.neg(a)` -> `-(a)`

If the value is not already an AQL value, it will be converted automatically.

### Ternary (if / else)

Creates a ternary expression from the given values.

`qb.if(condition, then, otherwise)` -> `(condition ? then : otherwise)`

If the values are not already AQL values, they will be converted automatically.

*Aliases:* `qb.if_(condition, then, otherwise)`

### Function Call

Creates a function call for the given name and arguments.

`qb.fn(name)(args…)`

If the values are not already AQL values, they will be converted automatically.

For built-in functions, methods with the relevant function name are already provided by the query builder.

*Examples*

* `qb.fn('MY::USER::FUNC')(1, 2, 3)` -> `MY::USER::FUNC(1, 2, 3)`
* `qb.fn('hello')()` -> `hello()`
* `qb.RANDOM()` -> `RANDOM()`
* `qb.FLOOR(qb.div(5, 2))` -> `FLOOR((5 / 2))`

## AQL Statements

In addition to the methods documented above, the query builder provides all methods of *PartialStatement* objects.

AQL *Statement* objects have a method *toAQL()* which returns their AQL representation as a JavaScript string.

*Examples*

```
qb.for('doc').in('my_collection').return('doc._key').toAQL()
// -> FOR doc IN my_collection RETURN doc._key
```

### FOR expression IN collection

`PartialStatement::for(expression).in(collection) : Statement`

*Alias:* `for_(expression).in_(collection)`

### LET varname = expression

`PartialStatement::let(varname, expression) : Statement`

*Alias:* `let_(varname, expression)`

### LET var1 = expr1, var2 = expr2, …, varn = exprn

`PartialStatement::let(definitions) : Statement`

*Alias:* `let_(definitions)`

### FILTER expression

`PartialStatement::filter(expression) : Statement`

### COLLECT varname = expression

`PartialStatement::collect(varname, expression) : Statement`

### COLLECT varname1 = expression INTO varname2

`PartialStatement::collect(varname1, expression).into(varname2) : Statement`

### COLLECT var1 = expr1, var2 = expr2, …, varn = exprn

`PartialStatement::collect(definitions) : Statement`

### COLLECT var1 = expr1, var2 = expr2, …, varn = exprn INTO varname

`PartialStatement::collect(definitions).into(varname) : Statement`

### COLLECT var1 = expr1, var2 = expr2, …, varn = exprn INTO varname COUNT

`PartialStatement::collect(definitions).into(varname).count() : Statement`

### COLLECT varname1 = expression KEEP varname2

`PartialStatement::collect(varname1, expression).keep(varname2) : Statement`

### SORT args…

`PartialStatement::sort(args…) : Statement`

### LIMIT offset, count

`PartialStatement::limit([offset,] count) : Statement`

### RETURN expression

`PartialStatement::return(expression) : Statement`

*Alias:* `return_(expression)`

### REMOVE expression IN collection

`PartialStatement::remove(expression).in(collection) : RemoveExpression`

*Aliases:*

* `remove(expression).in_(collection)`
* `remove(expression).into(collection)`

### REMOVE … LET varname = OLD RETURN varname

`RemoveExpression::returnOld(varname) : Statement`

### REMOVE … OPTIONS options

`RemoveExpression::options(options) : Statement`

### INSERT expression INTO collection

`PartialStatement::insert(expression).into(collection) : InsertExpression`

*Aliases:*

* `insert(expression).in(collection)`
* `insert(expression).in_(collection)`

### INSERT … OPTIONS options

`InsertExpression::options(options) : Statement`

### INSERT … LET varname = NEW RETURN varname

`InsertExpression::returnNew(varname) : Statement`

### UPDATE expression1 WITH expression2 IN collection

`PartialStatement::update(expression1).with(expression2).in(collection) : UpdateExpression`

*Aliases:*

* `update(expression1).with(expression2).in_(collection)`
* `update(expression1).with(expression2).into(collection)`
* `update(expression1).with_(expression2).in(collection)`
* `update(expression1).with_(expression2).in_(collection)`
* `update(expression1).with_(expression2).into(collection)`

### UPDATE expression IN collection

`PartialStatement::update(expression).in(collection) : UpdateExpression`

*Aliases:*

* `update(expression).in_(collection)`
* `update(expression).into(collection)`

### UPDATE … OPTIONS options

`UpdateExpression::options(options) : Statement`

### UPDATE … LET varname = NEW RETURN varname

`UpdateExpression::returnNew(varname) : Statement`

### UPDATE … LET varname = OLD RETURN varname

`UpdateExpression::returnOld(varname) : Statement`

### REPLACE expression1 WITH expression2 IN collection

`PartialStatement::replace(expression1).with(expression2).in(collection) : ReplaceExpression`

*Aliases:*

* `replace(expression1).with(expression2).in_(collection)`
* `replace(expression1).with(expression2).into(collection)`
* `replace(expression1).with_(expression2).in(collection)`
* `replace(expression1).with_(expression2).in_(collection)`
* `replace(expression1).with_(expression2).into(collection)`

### REPLACE expression IN collection

`PartialStatement::replace(expression).in(collection) : ReplaceExpression`

*Aliases:*

* `replace(expression).in_(collection)`
* `replace(expression).into(collection)`

### REPLACE … OPTIONS options

`ReplaceExpression::options(options) : Statement`

### REPLACE … LET varname = NEW RETURN varname

`ReplaceExpression::returnOld(varname) : Statement`

### REPLACE … LET varname = OLD RETURN varname

`ReplaceExpression::returnNew(varname) : Statement`

# License

The Apache License, Version 2.0. For more information, see the accompanying LICENSE file.
