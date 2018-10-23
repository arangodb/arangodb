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


### String

Wraps the given value as an AQL String literal.

`qb.str(value)`

If the value is not a JavaScript String, it will be converted first.

If the value is a quoted string, it will be treated as a string literal.

If the value is an object with a *toAQL* method, the result of calling that method will be wrapped instead.

**Examples**

* `23` => `"23"`
* `"some string"` => `"some string"`
* `'"some string"'` => `"\"some string\""`

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

If the value is already an AQL Object, its own value will be wrapped instead.

Any property values that are not already AQL values will be converted automatically.

Any keys that are quoted strings will be treated as string literals.

Any keys that start with the character "`:`" will be treated as dynamic properties and must be well-formed simple references.

Any other keys that need escaping will be quoted if necessary.

If you need to pass in raw JavaScript objects that shouldn't be converted according to these rules, you can use the `qb` function directly instead.

**Examples**

* `qb.obj({'some.name': 'value'})` => `{"some.name": value}`
* `qb.obj({hello: world})` => `{hello: world}`
* `qb.obj({'"hello"': world})` => `{"hello": world}`
* `qb.obj({':dynamic': 'props'})` => `{[dynamic]: props}`
* `qb.obj({': invalid': 'key'})` => throws an error (` invalid` is not a well-formed reference)

### Simple Reference

Wraps a given value in an AQL Simple Reference.

`qb.ref(value)`

If the value is not a JavaScript string or not a well-formed simple reference, an *AQLError* will be thrown.

If the value is an *ArangoCollection*, its *name* property will be used instead.

If the value is already an AQL Simple Reference, its value is wrapped instead.

**Examples**

Valid values:

* `foo`
* `foo.bar`
* `foo[*].bar`
* `foo.bar.QUX`
* `_foo._bar._qux`
* `foo1.bar2`
* `` `foo`.bar ``
* `` foo.`bar` ``

Invalid values:

* `1foo`
* `föö`
* `foo bar`
* `foo[bar]`

ArangoDB collection objects can be passed directly:

```js
var myUserCollection = applicationContext.collection('users');
var users = db._query(qb.for('u').in(myUserCollection).return('u')).toArray();
```

## AQL Expressions

### Range

Creates a range expression from the given values.

`qb.range(value1, value2)` => `value1..value2`

OR:

`aqlValue.range(value2)` => `value1..value2`

If the values are not already AQL values, they will be converted automatically.

*Alias:* `qb.to(value1, value2)`

**Examples**

`qb(2).to(5)` => `2..5`

### Property Access

Creates a property access expression from the given values.

`qb.get(obj, key)` => `obj[key]`

OR:

`aqlObj.get(key)` => `obj[key]`

If the values are not already AQL values, they will be converted automatically.

**Examples**

`qb.ref('x').get('y') => `x[y]`

### Raw Expression

Wraps a given value in a raw AQL expression.

`qb.expr(value)`

If the value is already an AQL Raw Expression, its value is wrapped instead.

**Warning:** Whenever possible, you should use one of the other methods or a combination thereof instead of using a raw expression. Raw expressions allow passing arbitrary strings into your AQL and thus will open you to AQL injection attacks if you are passing in untrusted user input.

## AQL Operations

### Boolean And

Creates an "and" operation from the given values.

`qb.and(a, b)` => `(a && b)`

OR:

`aqlValue.and(b)` => `(a && b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

**Examples**

`qb.ref('x').and('y')` => `(x && y)`

### Boolean Or

Creates an "or" operation from the given values.

`qb.or(a, b)` => `(a || b)`

OR:

`aqlValue.or(b)` => `(a || b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

**Examples**

`qb.ref('x').or('y')` => `(x || y)`

### Addition

Creates an addition operation from the given values.

`qb.add(a, b)` => `(a + b)`

OR:

`aqlValue.add(b)` => `(a + b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Alias:* `qb.plus(a, b)`

**Examples**

`qb.ref('x').plus('y')` => `(x + y)`

### Subtraction

Creates a subtraction operation from the given values.

`qb.sub(a, b)` => `(a - b)`

OR:

`aqlValue.sub(b)` => `(a - b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Alias:* `qb.minus(a, b)`

**Examples**

`qb.ref('x').minus('y')` => `(x - y)`

### Multiplication

Creates a multiplication operation from the given values.

`qb.mul(a, b)` => `(a * b)`

OR:

`aqlValue.mul(b)` => `(a * b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

*Alias:* `qb.times(a, b)`

**Examples**

`qb.ref('x').times('y')` => `(x * y)`

### Division

Creates a division operation from the given values.

`qb.div(a, b)` => `(a / b)`

OR:

`aqlValue.div(b)` => `(a / b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

**Examples**

`qb.ref('x').div('y')` => `(x / y)`

### Modulus

Creates a modulus operation from the given values.

`qb.mod(a, b)` => `(a % b)`

OR:

`aqlValue.mod(b)` => `(a % b)`

If the values are not already AQL values, they will be converted automatically.

This function can take any number of arguments.

**Examples**

`qb.ref('x').mod('y')` => `(x % y)`

### Equality

Creates an equality comparison from the given values.

`qb.eq(a, b)` => `(a == b)`

OR:

`qbValue.eq(b)` => `(a == b)`

If the values are not already AQL values, they will be converted automatically.

**Examples**

`qb.ref('x').eq('y')` => `(x == y)`

### Inequality

Creates an inequality comparison from the given values.

`qb.neq(a, b)` => `(a != b)`

OR:

`qbValue.neq(b)` => `(a != b)`

If the values are not already AQL values, they will be converted automatically.

**Examples**

`qb.ref('x').neq('y')` => `(x != y)`

### Greater Than

Creates a greater-than comparison from the given values.

`qb.gt(a, b)` => `(a > b)`

OR

`qbValue.gt(b)` => `(a > b)`

If the values are not already AQL values, they will be converted automatically.

**Examples**

`qb.ref('x').gt('y')` => `(x > y)`

### Greater Than Or Equal To

Creates a greater-than-or-equal-to comparison from the given values.

`qb.gte(a, b)` => `(a >= b)`

OR

`qbValue.gte(b)` => `(a >= b)`

If the values are not already AQL values, they will be converted automatically.

**Examples**

`qb.ref('x').gte('y')` => `(x >= y)`

### Less Than

Creates a less-than comparison from the given values.

`qb.lt(a, b)` => `(a < b)`

OR

`qbValue.lt(b)` => `(a < b)`

If the values are not already AQL values, they will be converted automatically.

**Examples**

`qb.ref('x').lt('y')` => `(x < y)`

### Less Than Or Equal To

Creates a less-than-or-equal-to comparison from the given values.

`qb.lte(a, b)` => `(a <= b)`

OR

`qbValue.lte(b)` => `(a <= b)`

If the values are not already AQL values, they will be converted automatically.

**Examples**

`qb.ref('x').lte('y')` => `(x <= y)`

### Contains

Creates an "in" comparison from the given values.

`qb.in(a, b)` => `(a in b)`

OR:

`qbValue.in(b)` => `(a in b)`

If the values are not already AQL values, they will be converted automatically.


**Examples**

`qb.ref('x').in('y')` => `(x in y)`

### Negation

Creates a negation from the given value.

`qb.not(a)` => `!(a)`

OR:

`qbValue.not()` => `!(a)`

If the value is not already an AQL value, it will be converted automatically.

**Examples**

`qb.not('x')` => `!(x)`

### Negative Value

Creates a negative value expression from the given value.

`qb.neg(a)` => `-(a)`

OR:

`qbValue.neg()` => `-(a)`

If the value is not already an AQL value, it will be converted automatically.

**Examples**

`qb.neg('x')` => `-(x)`

### Ternary (if / else)

Creates a ternary expression from the given values.

`qb.if(condition, thenDo, elseDo)` => `(condition ? thenDo : elseDo)`

OR:

`qbValue.then(thenDo).else(elseDo)` => `(condition ? thenDo : elseDo)`

If the values are not already AQL values, they will be converted automatically.

*Alias:* `qbValue.then(thenDo).otherwise(elseDo)`

**Examples**

`qb.ref('x').then('y').else('z')` => `(x ? y : z)`

### Function Call

Creates a functon call for the given name and arguments.

`qb.fn(name)(...args)`

If the values are not already AQL values, they will be converted automatically.

For built-in functions, methods with the relevant function name are already provided by the query builder.

**Examples**

* `qb.fn('MY::USER::FUNC')(1, 2, 3)` => `MY::USER::FUNC(1, 2, 3)`
* `qb.fn('hello')()` => `hello()`
* `qb.RANDOM()` => `RANDOM()`
* `qb.FLOOR(qb.div(5, 2))` => `FLOOR((5 / 2))`

## AQL Statements

In addition to the methods documented above, the query builder provides all methods of *PartialStatement* objects.

AQL *Statement* objects have a method *toAQL()* which returns their AQL representation as a JavaScript string.

**Examples**

```js
qb.for('doc').in('my_collection').return('doc._key').toAQL()
// => FOR doc IN my_collection RETURN doc._key
```

### FOR expression IN collection

`PartialStatement::for(expression).in(collection) : PartialStatement`


**Examples**

* `_.for('doc').in('my_collection')` => `FOR doc IN my_collection`

### LET varname = expression

`PartialStatement::let(varname, expression) : PartialStatement`


**Examples**

* `_.let('foo', 23)` => `LET foo = 23`

### LET var1 = expr1, var2 = expr2, …, varn = exprn

`PartialStatement::let(definitions) : PartialStatement`


**Examples**

* `_.let({a: 1, b: 2, c: 3})` => `LET a = 1, b = 2, c = 3`

### RETURN expression

`PartialStatement::return(expression) : ReturnExpression`


**Examples**

* `_.return('x')` => `RETURN x`
* `_.return({x: 'x'})` => `RETURN {x: x}`

### RETURN DISTINCT expression

`PartialStatement::returnDistinct(expression) : ReturnExpression`

**Examples**

* `_.returnDistinct('x')` => `RETURN DISTINCT x`

### FILTER expression

`PartialStatement::filter(expression) : PartialStatement`

**Examples**

* `_.filter(qb.eq('a', 'b'))` => `FILTER a == b`

### COLLECT …

#### COLLECT WITH COUNT INTO varname

`PartialStatement::collectWithCountInto(varname) : CollectExpression`

**Examples**

* `_.collectWithCountInto('x')` => `COLLECT WITH COUNT INTO x`

#### COLLECT varname = expression

`PartialStatement::collect(varname, expression) : CollectExpression`

**Examples**

* `_.collect('x', 'y')` => `COLLECT x = y`

#### COLLECT var1 = expr1, var2 = expr2, …, varn = exprn

`PartialStatement::collect(definitions) : CollectExpression`

**Examples**

* `_.collect({x: 'a', y: 'b'})` => `COLLECT x = a, y = b`

#### … WITH COUNT INTO varname

`CollectExpression::withCountInto(varname) : CollectExpression`

**Examples**

* `_.withCountInto('x')` => `WITH COUNT INTO x`

#### … INTO varname

`CollectExpression::into(varname) : CollectExpression`

**Examples**

* `_.into('z')` => `INTO z`

##### … KEEP ...vars

`CollectExpression::keep(...vars) : CollectExpression`

**Examples**

* `_.into('z').keep('a', 'b')` => `INTO z KEEP a, b`

#### … INTO varname = expression

`CollectExpression::into(varname, expression) : CollectExpression`

**Examples**

* `_.into('x', 'y')` => `INTO x = y`

#### … OPTIONS options

`CollectExpression::options(options) : CollectExpression`

**Examples**

* `_.options('opts')` => `OPTIONS opts`

### … SORT ...args

`PartialStatement::sort(...args) : PartialStatement`

**Examples**

* `_.sort('x', 'DESC', 'y', 'ASC')` => `SORT x DESC, y ASC`

### … LIMIT offset, count

`PartialStatement::limit([offset,] count) : PartialStatement`

**Examples**

* `_.limit(20)` => `LIMIT 20`
* `_.limit(20, 20)` => `LIMIT 20, 20`

### REMOVE …

#### REMOVE expression IN collection

`PartialStatement::remove(expression).in(collection) : RemoveExpression`

*Alias:* `remove(expression).into(collection)`

**Examples**

* `_.remove('x').in('y')` => `REMOVE x IN y`

#### … LET varname = OLD RETURN varname

`RemoveExpression::returnOld(varname) : ReturnExpression`

**Examples**

* `_.returnOld('z')` => `LET z = OLD RETURN z`

#### … OPTIONS options

`RemoveExpression::options(options) : RemoveExpression`

**Examples**

* `_.options('opts')` => `OPTIONS opts`

### UPSERT …

#### UPSERT expression1 INSERT expression2 REPLACE expression3 IN collection

`PartialStatement::upsert(expression1).insert(expression2).replace(expression3).in(collection) : UpsertExpression`

*Alias:* `….into(collection)`

**Examples**

* `_.upsert('x').insert('y').replace('z').in('c')` => `UPSERT x INSERT y REPLACE z IN c`

#### UPSERT expression1 INSERT expression2 UPDATE expression3 IN collection

`PartialStatement::upsert(expression1).insert(expression2).update(expression3).in(collection) : UpsertExpression`

*Alias:* `….into(collection)`

**Examples**

* `_.upsert('x').insert('y').update('z').in('c')` => `UPSERT x INSERT y UPDATE z IN c`

#### … OPTIONS options

`UpsertExpression::options(options) : UpsertExpression`

**Examples**

* `_.options('opts')` => `OPTIONS opts`

### INSERT …

#### INSERT expression INTO collection

`PartialStatement::insert(expression).into(collection) : InsertExpression`

*Alias:* `insert(expression).in(collection)`

**Examples**

* `_.insert('x').into('y')` => `INSERT x INTO y`

#### … OPTIONS options

`InsertExpression::options(options) : InsertExpression`

**Examples**

* `_.options('opts')` => `OPTIONS opts`

#### … LET varname = NEW RETURN varname

`InsertExpression::returnNew(varname) : ReturnExpression`

**Examples**

* `_.returnNew('z')` => `LET z = NEW RETURN z`

### UPDATE …

#### UPDATE expression IN collection

`PartialStatement::update(expression).in(collection) : UpdateExpression`

*Alias:* `update(expression).into(collection)`

**Examples**

* `_.update('x').in('y')` => `UPDATE x IN y`

#### UPDATE expression1 WITH expression2 IN collection

`PartialStatement::update(expression1).with(expression2).in(collection) : UpdateExpression`

*Alias:* `update(expression1).with(expression2).into(collection)`

**Examples**

* `_.update('x').with('y').in('z')` => `UPDATE x WITH y IN z`

#### … OPTIONS options

`UpdateExpression::options(options) : UpdateExpression`

**Examples**

* `_.options('opts')` => `OPTIONS opts`

#### … LET varname = NEW RETURN varname

`UpdateExpression::returnNew(varname) : ReturnExpression`

**Examples**

* `_.returnNew('z')` => `LET z = NEW RETURN z`

#### … LET varname = OLD RETURN varname

`UpdateExpression::returnOld(varname) : ReturnExpression`

**Examples**

* `_.returnOld('z')` => `LET z = OLD RETURN z`

### REPLACE …

#### REPLACE expression IN collection

`PartialStatement::replace(expression).in(collection) : ReplaceExpression`

*Alias:* `replace(expression).into(collection)`

**Examples**

* `_.replace('x').in('y')` => `REPLACE x IN y`

#### REPLACE expression1 WITH expression2 IN collection

`PartialStatement::replace(expression1).with(expression2).in(collection) : ReplaceExpression`

*Alias:* `replace(expression1).with(expression2).into(collection)`

**Examples**

* `_.replace('x').with('y').in('z')` => `REPLACE x WITH y IN z`

#### … OPTIONS options

`ReplaceExpression::options(options) : ReplaceExpression`

**Examples**

* `_.options('opts')` => `OPTIONS opts`

#### … LET varname = NEW RETURN varname

`ReplaceExpression::returnOld(varname) : ReturnExpression`

**Examples**

* `_.returnNew('z')` => `LET z = NEW RETURN z`

#### … LET varname = OLD RETURN varname

`ReplaceExpression::returnNew(varname) : ReturnExpression`

**Examples**

* `_.returnOld('z')` => `LET z = OLD RETURN z`

# License

The Apache License, Version 2.0. For more information, see the accompanying LICENSE file.
