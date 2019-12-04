# expand-template

> Expand placeholders in a template string.

[![Build Status](https://travis-ci.org/ralphtheninja/expand-template.svg?branch=master)](https://travis-ci.org/ralphtheninja/expand-template)
[![Greenkeeper badge](https://badges.greenkeeper.io/ralphtheninja/expand-template.svg)](https://greenkeeper.io/)

## Install

```
$ npm i expand-template -S
```

## Usage

Default functionality expands templates using `{}` as separators for string placeholders.

```js
var expand = require('expand-template')()
var template = '{foo}/{foo}/{bar}/{bar}'
console.log(expand(template, {
  foo: 'BAR',
  bar: 'FOO'
}))
// -> BAR/BAR/FOO/FOO
```

Custom separators:

```js
var expand = require('expand-template')({ sep: '[]' })
var template = '[foo]/[foo]/[bar]/[bar]'
console.log(expand(template, {
  foo: 'BAR',
  bar: 'FOO'
}))
// -> BAR/BAR/FOO/FOO
```

## License
All code, unless stated otherwise, is dual-licensed under [`WTFPL`](http://www.wtfpl.net/txt/copying/) and [`MIT`](https://opensource.org/licenses/MIT).
