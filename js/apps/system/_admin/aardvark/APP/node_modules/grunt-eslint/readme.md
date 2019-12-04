# grunt-eslint [![Build Status](https://travis-ci.org/sindresorhus/grunt-eslint.svg?branch=master)](https://travis-ci.org/sindresorhus/grunt-eslint)

> Validate files with [ESLint](http://eslint.org)

![](screenshot.png)


## Install

```
$ npm install --save-dev grunt-eslint
```


## Usage

```js
require('load-grunt-tasks')(grunt); // npm install --save-dev load-grunt-tasks

grunt.initConfig({
	eslint: {
		target: ['file.js']
	}
});

grunt.registerTask('default', ['eslint']);
```


## Examples

### Custom config and rules

```js
grunt.initConfig({
	eslint: {
		options: {
			configFile: 'conf/eslint.json',
			rulePaths: ['conf/rules']
		},
		target: ['file.js']
	}
});
```

### Custom formatter

```js
grunt.initConfig({
	eslint: {
		options: {
			format: require('eslint-tap')
		},
		target: ['file.js']
	}
});
```


## Options

See the [ESLint options](http://eslint.org/docs/developer-guide/nodejs-api#cliengine).

In addition the following options are supported:

### format

Type: `string`<br>
Default: `'stylish'`

Name of a [built-in formatter](https://github.com/nzakas/eslint/tree/master/lib/formatters) or path to a custom one.

Some formatters you might find useful: [eslint-json](https://github.com/sindresorhus/eslint-json), [eslint-tap](https://github.com/sindresorhus/eslint-tap).

### outputFile

Type: `string`<br>
Default: `''`

Output the report to a file.

### quiet

Type: `boolean`<br>
Default: `false`

Report errors only.

### maxWarnings

Type: `number`<br>
Default: `-1` *(means no limit)*

Number of warnings to trigger non-zero exit code.


## License

MIT © [Sindre Sorhus](https://sindresorhus.com)
