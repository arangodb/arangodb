# maxmin [![Build Status](https://travis-ci.org/sindresorhus/maxmin.svg?branch=master)](https://travis-ci.org/sindresorhus/maxmin)

> Get a pretty output of the original, minified, gzipped size of a string or buffer

![](screenshot.png)

Useful for logging the difference between original and minified file in e.g. a build-system.


## Install

```sh
$ npm install --save maxmin
```


## Usage

```js
var maxmin = require('maxmin');

var max = 'function smoothRangeRandom(min,max){var num=Math.floor(Math.random()*(max-min+1)+min);return this.prev=num===this.prev?++num:num};';

var min = '(function(b,c){var a=Math.floor(Math.random()*(c-b+1)+b);return this.a=a===this.a?++a:a})()';

console.log(maxmin(max, min, true));
//=> 130 B → 91 B → 53 B (gzip)
```

## API

### maxmin(max, min, useGzip)

#### max

*Required*  
Type: `string`, `buffer`, `number`

Original string or its size in bytes.

#### min

*Required*  
Type: `string`, `buffer`, `number`

Minified string or its size in bytes.

#### useGzip

Type: `boolean`  
Default: `false`

Show gzipped size of `min`. Pretty slow. Not shown when `min` is a `number`.


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
