# gzip-size [![Build Status](https://travis-ci.org/sindresorhus/gzip-size.svg?branch=master)](https://travis-ci.org/sindresorhus/gzip-size)

> Get the gzipped size of a string or buffer


## Install

```sh
$ npm install --save gzip-size
```


## Usage

```js
var gzipSize = require('gzip-size');
var string = 'Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus.';

console.log(string.length);
//=> 191

console.log(gzipSize.sync(string));
//=> 78
```


## API

### gzipSize(input, callback)

#### input

*Required*  
Type: `string`, `buffer`

#### callback(err, size)

*Required*  
Type: `function`

### gzipSize.sync(input)

Returns the size.

#### input

*Required*  
Type: `string`, `buffer`  


## CLI

```sh
$ npm install --global gzip-size
```

```sh
$ gzip-size --help

  Usage
    gzip-size <file>
    cat <file> | gzip-size

  Example
    gzip-size index.js
    211
```

### Tip

Combine it with [pretty-bytes](https://github.com/sindresorhus/pretty-bytes) to get a human readable output:

```sh
$ pretty-bytes $(gzip-size jquery.min.js)
29.34 kB
```


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
