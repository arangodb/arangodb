# dargs [![Build Status](https://travis-ci.org/sindresorhus/dargs.svg?branch=master)](https://travis-ci.org/sindresorhus/dargs)

> Reverse [`minimist`](https://github.com/substack/minimist). Convert an object of options into an array of command-line arguments.

Useful when spawning command-line tools.


## Install

```
$ npm install --save dargs
```


#### Usage

```js
var dargs = require('dargs');

var input = {
	foo: 'bar',
	hello: true,                    // results in only the key being used
	cake: false,                    // prepends `no-` before the key
	camelCase: 5,                   // camelCase is slugged to `camel-case`
	multiple: ['value', 'value2'],  // converted to multiple arguments
	sad: ':('
};

var excludes = ['sad'];
var includes = ['camelCase', 'multiple', 'sad'];

console.log(dargs(input, {excludes: excludes}));
/*
[
	'--foo=bar',
	'--hello',
	'--no-cake',
	'--camel-case=5',
	'--multiple=value',
	'--multiple=value2'
]
*/

console.log(dargs(input, {
	excludes: excludes,
	includes: includes
}));
/*
[
	'--camel-case=5',
	'--multiple=value',
	'--multiple=value2'
]
*/


console.log(dargs(input, {includes: includes}));
/*
[
	'--camel-case=5',
	'--multiple=value',
	'--multiple=value2',
	'--sad=:(''
]
*/
```

## API

### dargs(input, options)

#### input

*Required*  
Type: `object`

Object to convert to command-line arguments.

#### options

Type: `object`

##### excludes

Type: `array`

Keys to exclude. Takes precedence over `includes`.

##### includes

Type: `array`

Keys to include.

##### ignoreFalse

Type: `boolean`  
Default: `false`

Don't include `false` values. This is mostly useful when dealing with strict argument parsers that would throw on unknown arguments like `--no-foo`.


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
