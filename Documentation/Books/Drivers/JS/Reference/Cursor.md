<!-- don't edit here, its from https://@github.com/arangodb/arangodbjs.git / docs/Drivers/ -->
# Cursor API

_Cursor_ instances provide an abstraction over the HTTP API's limitations.
Unless a method explicitly exhausts the cursor, the driver will only fetch as
many batches from the server as necessary. Like the server-side cursors,
_Cursor_ instances are incrementally depleted as they are read from.

```js
const db = new Database();
const cursor = await db.query('FOR x IN 1..5 RETURN x');
// query result list: [1, 2, 3, 4, 5]
const value = await cursor.next();
assert.equal(value, 1);
// remaining result list: [2, 3, 4, 5]
```

## cursor.count

`cursor.count: number`

The total number of documents in the query result. This is only available if the
`count` option was used.

## cursor.all

`async cursor.all(): Array<Object>`

Exhausts the cursor, then returns an array containing all values in the cursor's
remaining result list.

**Examples**

```js
const cursor = await db._query('FOR x IN 1..5 RETURN x');
const result = await cursor.all()
// result is an array containing the entire query result
assert.deepEqual(result, [1, 2, 3, 4, 5]);
assert.equal(cursor.hasNext(), false);
```

## cursor.next

`async cursor.next(): Object`

Advances the cursor and returns the next value in the cursor's remaining result
list. If the cursor has already been exhausted, returns `undefined` instead.

**Examples**

```js
// query result list: [1, 2, 3, 4, 5]
const val = await cursor.next();
assert.equal(val, 1);
// remaining result list: [2, 3, 4, 5]

const val2 = await cursor.next();
assert.equal(val2, 2);
// remaining result list: [3, 4, 5]
```

## cursor.hasNext

`cursor.hasNext(): boolean`

Returns `true` if the cursor has more values or `false` if the cursor has been
exhausted.

**Examples**

```js
await cursor.all(); // exhausts the cursor
assert.equal(cursor.hasNext(), false);
```

## cursor.each

`async cursor.each(fn): any`

Advances the cursor by applying the function _fn_ to each value in the cursor's
remaining result list until the cursor is exhausted or _fn_ explicitly returns
`false`.

Returns the last return value of _fn_.

Equivalent to _Array.prototype.forEach_ (except async).

**Arguments**

* **fn**: `Function`

  A function that will be invoked for each value in the cursor's remaining
  result list until it explicitly returns `false` or the cursor is exhausted.

  The function receives the following arguments:

  * **value**: `any`

    The value in the cursor's remaining result list.

  * **index**: `number`

    The index of the value in the cursor's remaining result list.

  * **cursor**: `Cursor`

    The cursor itself.

**Examples**

```js
const results = [];
function doStuff(value) {
  const VALUE = value.toUpperCase();
  results.push(VALUE);
  return VALUE;
}

const cursor = await db.query('FOR x IN ["a", "b", "c"] RETURN x')
const last = await cursor.each(doStuff);
assert.deepEqual(results, ['A', 'B', 'C']);
assert.equal(cursor.hasNext(), false);
assert.equal(last, 'C');
```

## cursor.every

`async cursor.every(fn): boolean`

Advances the cursor by applying the function _fn_ to each value in the cursor's
remaining result list until the cursor is exhausted or _fn_ returns a value that
evaluates to `false`.

Returns `false` if _fn_ returned a value that evaluates to `false`, or `true`
otherwise.

Equivalent to _Array.prototype.every_ (except async).

**Arguments**

* **fn**: `Function`

  A function that will be invoked for each value in the cursor's remaining
  result list until it returns a value that evaluates to `false` or the cursor
  is exhausted.

  The function receives the following arguments:

  * **value**: `any`

    The value in the cursor's remaining result list.

  * **index**: `number`

    The index of the value in the cursor's remaining result list.

  * **cursor**: `Cursor`

    The cursor itself.

```js
const even = value => value % 2 === 0;

const cursor = await db.query('FOR x IN 2..5 RETURN x');
const result = await cursor.every(even);
assert.equal(result, false); // 3 is not even
assert.equal(cursor.hasNext(), true);

const value = await cursor.next();
assert.equal(value, 4); // next value after 3
```

## cursor.some

`async cursor.some(fn): boolean`

Advances the cursor by applying the function _fn_ to each value in the cursor's
remaining result list until the cursor is exhausted or _fn_ returns a value that
evaluates to `true`.

Returns `true` if _fn_ returned a value that evaluates to `true`, or `false`
otherwise.

Equivalent to _Array.prototype.some_ (except async).

**Examples**

```js
const even = value => value % 2 === 0;

const cursor = await db.query('FOR x IN 1..5 RETURN x');
const result = await cursor.some(even);
assert.equal(result, true); // 2 is even
assert.equal(cursor.hasNext(), true);

const value = await cursor.next();
assert.equal(value, 3); // next value after 2
```

## cursor.map

`cursor.map(fn): Array<any>`

Advances the cursor by applying the function _fn_ to each value in the cursor's
remaining result list until the cursor is exhausted.

Returns an array of the return values of _fn_.

Equivalent to _Array.prototype.map_ (except async).

**Note**: This creates an array of all return values. It is probably a bad idea
to do this for very large query result sets.

**Arguments**

* **fn**: `Function`

  A function that will be invoked for each value in the cursor's remaining
  result list until the cursor is exhausted.

  The function receives the following arguments:

  * **value**: `any`

    The value in the cursor's remaining result list.

  * **index**: `number`

    The index of the value in the cursor's remaining result list.

  * **cursor**: `Cursor`

    The cursor itself.

**Examples**

```js
const square = value => value * value;
const cursor = await db.query('FOR x IN 1..5 RETURN x');
const result = await cursor.map(square);
assert.equal(result.length, 5);
assert.deepEqual(result, [1, 4, 9, 16, 25]);
assert.equal(cursor.hasNext(), false);
```

## cursor.reduce

`cursor.reduce(fn, [accu]): any`

Exhausts the cursor by reducing the values in the cursor's remaining result list
with the given function _fn_. If _accu_ is not provided, the first value in the
cursor's remaining result list will be used instead (the function will not be
invoked for that value).

Equivalent to _Array.prototype.reduce_ (except async).

**Arguments**

* **fn**: `Function`

  A function that will be invoked for each value in the cursor's remaining
  result list until the cursor is exhausted.

  The function receives the following arguments:

  * **accu**: `any`

    The return value of the previous call to _fn_. If this is the first call,
    _accu_ will be set to the _accu_ value passed to _reduce_ or the first value
    in the cursor's remaining result list.

  * **value**: `any`

    The value in the cursor's remaining result list.

  * **index**: `number`

    The index of the value in the cursor's remaining result list.

  * **cursor**: `Cursor`

    The cursor itself.

**Examples**

```js
const add = (a, b) => a + b;
const baseline = 1000;

const cursor = await db.query('FOR x IN 1..5 RETURN x');
const result = await cursor.reduce(add, baseline)
assert.equal(result, baseline + 1 + 2 + 3 + 4 + 5);
assert.equal(cursor.hasNext(), false);

// -- or --

const result = await cursor.reduce(add);
assert.equal(result, 1 + 2 + 3 + 4 + 5);
assert.equal(cursor.hasNext(), false);
```
