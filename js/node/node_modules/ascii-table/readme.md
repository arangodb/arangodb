Ascii Table
===========

[![Build Status](https://secure.travis-ci.org/sorensen/ascii-table.png)](http://travis-ci.org/sorensen/ascii-table) 
[![devDependency Status](https://david-dm.org/sorensen/ascii-table.png)](https://david-dm.org/sorensen/ascii-table#info=dependencies)
[![NPM version](https://badge.fury.io/js/ascii-table.png)](http://badge.fury.io/js/ascii-table)

Easy table output for node debugging, but you could probably do more with it, 
since its just a string.

Table of Contents
-----------------

* [Usage](#usage)
* [Example](#usage)
* [API](#api)
  - [Static Methods](#static-methods)
    * [factory([title])](#asciitablefactorytitle)
    * [align(direction, val, len, [pad])](#asciitablealigndirection-val-len-pad)
    * [alignLeft(val, len, [pad])](#asciitablealignleftval-len-pad)
    * [alignCenter(val, len, [pad])](#asciitablealigncenterval-len-pad)
    * [alignRight(val, len, [pad])](#asciitablealignrightval-len-pad)
    * [alignAuto(val, len, [pad])](#asciitablealignautoval-len-pad)
    * [arrayFill(len, [val])](#asciitablearrayfilllen-val)
  - [Instance Methods](#instance-methods)
    * [setBorder([edge], [fill], [top], [bottom])](#instancesetborderedge-fill-top-bottom)
    * [removeBorder()](#instanceremoveborder)
    * [setAlign(idx, direction)](#instancesetalignidx-direction)
    * [setAlignLeft(idx)](#instancesetalignleftidx)
    * [setAlignCenter(idx)](#instancesetaligncenteridx)
    * [setAlignRight(idx)](#instancesetalignrightidx)
    * [setTitle(title)](#instancesettitletitle)
    * [getTitle()](#instancegettitle)
    * [setTitleAlign(direction)](#instancesettitlealigndirection)
    * [setTitleAlignLeft()](#instancesettitlealignleft)
    * [setTitleAlignCenter()](#instancesettitlealigncenter)
    * [setTitleAlignRight()](#instancesettitlealignright)
    * [sort([iterator])](#instancesortiterator)
    * [sortColumn(idx, [iterator])](#instancesortcolumnidx-iterator)
    * [setHeading(heading, [...])](#instancesetheadingheading)
    * [setHeadingAlign(direction)](#instancesetheadingaligndirection)
    * [setHeadingAlignLeft()](#instancesetheadingalignleft)
    * [setHeadingAlignCenter()](#instancesetheadingaligncenter)
    * [setHeadingAlignRight()](#instancesetheadingalignright)
    * [addRow(row, [...])](#instanceaddrowrow)
    * [addRowMatrix(rows)](#instanceaddrowmatrixrows)
    * [setJustify([enabled])](#instancesetjustifyenabled)
    * [toString()](#instancetostring)
    * [toJSON()](#instancetojson)
    * [fromJSON(obj)](#instancefromjsonobj)
    * [clear()](#instanceclear)
    * [clearRows()](#instanceclearrows)
* [Install](#install)
* [Contributors](#contributors)
* [License](#license)

Usage
-----

Node.js

```js
var AsciiTable = require('ascii-table')
```

Browser

```html
<script src="ascii-table.min.js"></script>
```

*Note*: If using in the browser, it will be placed under `window.AsciiTable`


Example
-------

Basic usage

```js
var table = new AsciiTable('A Title')
table
  .setHeading('', 'Name', 'Age')
  .addRow(1, 'Bob', 52)
  .addRow(2, 'John', 34)
  .addRow(3, 'Jim', 83)

console.log(table.toString())
```

```
.----------------.
|    A Title     |
|----------------|
|   | Name | Age |
|---|------|-----|
| 1 | Bob  |  52 |
| 2 | John |  34 |
| 3 | Jim  |  83 |
'----------------'
```

We can make a simple table without a title or headings as well.

```js
var table = new AsciiTable()

table
  .addRow('a', 'apple', 'Some longer string')
  .addRow('b', 'banana', 'hi')
  .addRow('c', 'carrot', 'meow')
  .addRow('e', 'elephants')


console.log(table.toString())
```

```
.------------------------------------.
| a | apple     | Some longer string |
| b | banana    | hi                 |
| c | carrot    | meow               |
| e | elephants |                    |
'------------------------------------'
```


API
---

### Static Methods

#### AsciiTable

See: `AsciiTable.factory` for details on instantiation

#### AsciiTable.factory([title], [options])

Table instance creator

* `title` - table title (optional, default `null`)
* `options` - table options (optional)
  - `prefix` - string prefix to add to each line on render

***Note:*** If an object is passed in place of the `title`, the `fromJSON` 
method will be used to populate the table.

Example:

```js
var table = AsciiTable.factory('title')

var table = AsciiTable.factory({
  title: 'Title'
, heading: [ 'id', 'name' ]
, rows: [ 
    [ 1, 'Bob' ]
  , [ 2, 'Steve' ] 
  ] 
})
```


#### AsciiTable.align(direction, val, len, [pad])

Shortcut to one of the three following methods

* `direction` - alignment direction (`AsciiTable.LEFT`, `AsciiTable.CENTER`, `AsciiTable.RIGHT`)
* `val` - string to align
* `len` - total length of created string
* `pad` - padding / fill char (optional, default `' '`)

Example:

```js
table.align(AsciiTable.LEFT, 'hey', 7) // 'hey    '
```


#### AsciiTable.alignLeft(val, len, [pad])

* `val` - string to align
* `len` - total length of created string
* `pad` - padding / fill char (optional, default `' '`)

Example:

```js
table.alignLeft('hey', 7, '-') // 'hey----'
```


#### AsciiTable.alignCenter(val, len, [pad])

* `val` - string to align
* `len` - total length of created string
* `pad` - padding / fill char (optional, default `' '`)

Example:

```js
table.alignCenter('hey', 7) // '  hey  '
```


#### AsciiTable.alignRight(val, len, [pad])

* `val` - string to align
* `len` - total length of created string
* `pad` - padding / fill char (optional, default `' '`)

Example:

```js
table.alignRight('hey', 7) // '    hey'
```


#### AsciiTable.alignAuto(val, len, [pad])

Attempt to do intelligent alignment of provided `val`, `String` input will 
be left aligned, `Number` types will be right aligned.

* `val` - string to align
* `len` - total length of created string
* `pad` - padding / fill char (optional, default `' '`)

Example:

```js
table.align(AsciiTable.LEFT, 'hey', 7) // 'hey    '
```


#### AsciiTable.arrayFill(len, [val])

Create a new array at the given len, filled with the given value, mainly used internally

* `len` - length of array
* `val` - fill value (optional)

Example:

```js
AsciiTable.arrayFill(4, 0) // [0, 0, 0, 0]
```

### Instance Methods

#### instance.setBorder([edge], [fill], [top], [bottom])

Set the border characters for rendering, if no arguments are passed it will be 
reset to defaults. If a single `edge` arg is passed, it will be used for all borders.

* `edge` - horizontal edges (optional, default `|`)
* `fill` - vertical edges (optional, default `-`)
* `top` - top corners (optional, default `.`)
* `bottom` - bottom corners (optional, default `'`)

Example:

```js
var table = new AsciiTable('Stars')
table
  .setBorder('*')
  .setHeading('oh', 'look')
  .addRow('so much', 'star power')

console.log(table.toString())
```

```
************************
*        Stars         *
************************
*   oh    *    look    *
************************
* so much * star power *
************************
```


#### instance.removeBorder()

Example:

```js
table.removeBorder()

console.log('' + table)
```

```
  #     Fruit           Thing
 --- ----------- --------------------
  a       apple   Some longer string
  b      banana           hi
  c      carrot          meow
  e   elephants
```


#### instance.setAlign(idx, direction)

* `idx` - column index to align
* `direction` - alignment direction, (`AsciiTable.LEFT`, `AsciiTable.CENTER`, `AsciiTable.RIGHT`)

Example:

```js
table
  .setAlign(2, AsciiTable.RIGHT)
  .setAlign(1, AsciiTable.CENTER)

console.log(table.toString())
```

```
.-------------------------------------.
| a  |   apple   | Some longer string |
| b  |   banana  |                 hi |
| c  |   carrot  |               meow |
| e  | elephants |                    |
'-------------------------------------'
```


#### instance.setAlignLeft(idx)

Alias to `instance.setAlign(idx, AsciiTable.LEFT)`


#### instance.setAlignCenter(idx)

Alias to `instance.setAlign(idx, AsciiTable.CENTER)`


#### instance.setAlignRight(idx)

Alias to `instance.setAlign(idx, AsciiTable.RIGHT)`


#### instance.setTitle(title)

* `title` - table title

Example:

```js
var table = new AsciiTable('Old Title')

table.setTitle('New Title')
```

#### instance.getTitle()

Get the current title of the table

Example:

```js
table.getTitle() // 'New Title'
```


#### instance.setTitleAlign(direction)

* `direction` - table alignment direction

Example:

```js
```


#### instance.setTitleAlignLeft()

Alias to `instance.setTitleAlign(AsciiTable.LEFT)`


#### instance.setTitleAlignCenter()

Alias to `instance.setTitleAlign(AsciiTable.CENTER)`


#### instance.setTitleAlignRight()

Alias to `instance.setTitleAlign(AsciiTable.RIGHT)`


#### instance.sort(iterator)

* `iterator` - sorting method to run against the rows

Example:

```js
table.sort(function(a, b) {
  return a[2] - b[2]
})
console.log(table.toString())
```

```
.----------------.
| 2 | John |  34 |
| 1 | Bob  |  52 |
| 3 | Jim  |  83 |
'----------------'
```


#### instance.sortColumn(index, iterator)

Sorting shortcut for targeting a specific column

* `index` - column idx to sort
* `iterator` - sorting method to run against column values

Example:

```js
// This is quivalent to the `sort` example above
table.sortColumn(2, function(a, b) {
  return a - b
})
```


#### instance.setHeading(heading, [...])

Set the column headings for the table, takes arguments the same way as `addRow`

* `heading` - heading array or arguments

Example:

```js
table.setHeading('ID', 'Key', 'Value')

// or:

table.setHeading(['ID', 'Key', 'Value'])
```


#### instance.setHeadingAlign(direction)

* `direction` - 

Example:

```js
```


#### instance.setHeadingAlignLeft()

Alias to `instance.setHeadingAlignLeft(AsciiTable.LEFT)`


#### instance.setHeadingAlignCenter()

Alias to `instance.setHeadingAlignLeft(AsciiTable.CENTER)`


#### instance.setHeadingAlignRight()

Alias to `instance.setHeadingAlignLeft(AsciiTable.RIGHT)`


#### instance.addRow(row, [...])

Rows can be added using a single array argument, or the arguments if multiple 
args are used when calling the method.

* `row` - array or arguments of column values

Example:

```js
var table = new AsciiTable()

table
  .addRow(1, 'Bob', 52)
  .addRow([2, 'John', 34])

console.log(table.render())
```

```
.---------------.
| 1 | Bob  | 52 |
| 2 | John | 34 |
'---------------'
```


#### instance.addRowMatrix(rows)

Bulk `addRow` operation

* `rows` - multidimentional array of rows

Example:

```js
table.addRowMatrix([
  [2, 'John', 34]
, [3, 'Jim', 83]
])

```


#### instance.setJustify(enabled)

Justify all columns to be the same width

* `enabled` - boolean for turning justify on or off, `undefined` considered true

Example:

```js
table
  .addRow('1', 'two', 'three')
  .setJustify()

console.log(table.toString())
```

```
.-----------------------.
| 1     | two   | three |
'-----------------------'
```


#### instance.toString()

Render the instance as a string for output

**Alias**: [`valueOf`, `render`]


#### instance.toJSON()

Return the JSON representation of the table, this also allows us to call 
`JSON.stringify` on the instance.

Example:

```js
var table = new AsciiTable('Title')

table
  .setHeading('id', 'name')
  .addRow(1, 'Bob')
  .addRow(2, 'Steve')

console.log(table.toJSON())
console.log(JSON.stringify(table))
```

```js
{
  title: 'Title'
, heading: [ 'id', 'name' ]
, rows: [ 
    [ 1, 'Bob' ]
  , [ 2, 'Steve' ] 
  ] 
}
```

```
{"title":"Title","heading":["id","name"],"rows":[[1,"Bob"],[2,"Steve"]]}
```


#### instance.fromJSON(obj)

Populate the table from json object, should match the `toJSON` output above.

**Alias**: [`parse`]

Example:

```js
var table = new AsciiTable().fromJSON({
  title: 'Title'
, heading: [ 'id', 'name' ]
, rows: [ 
    [ 1, 'Bob' ]
  , [ 2, 'Steve' ] 
  ] 
})
```


#### instance.clear()

Clear / reset all table data

**Alias**: [`reset`]


#### instance.clearRows()

Reset all row data, maintains title and headings.



Install
-------

With [npm](https://npmjs.org)

```
npm install ascii-table
```


Contributors
------------

[Matthew Oliveira](https://github.com/m4olivei)
[Peter Daum](https://github.com/coders-kitchen)


License
-------

(The MIT License)

Copyright (c) 2013 Beau Sorensen

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
