# get-stdin [![Build Status](https://travis-ci.org/sindresorhus/get-stdin.svg?branch=master)](https://travis-ci.org/sindresorhus/get-stdin)

> Easier stdin


## Install

```sh
$ npm install --save get-stdin
```


## Usage

```js
// example.js
var stdin = require('get-stdin');

stdin(function (data) {
	console.log(data);
	//=> unicorns
});
```

```sh
$ echo unicorns | node example.js
unicorns
```


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
