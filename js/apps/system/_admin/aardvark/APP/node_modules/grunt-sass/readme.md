# grunt-sass [![Build Status](https://travis-ci.org/sindresorhus/grunt-sass.svg?branch=master)](https://travis-ci.org/sindresorhus/grunt-sass)

[<img src="https://rawgit.com/sass/node-sass/master/media/logo.svg" width="150" align="right">](https://github.com/sass/node-sass)

> Compile Sass to CSS using [node-sass](https://github.com/sass/node-sass)

*The issue tracker is disabled because of continuous abuse. Use [Stack Overflow](https://stackoverflow.com/questions/tagged/node-sass) for support questions. Issues with the output should be reported on the libsass [issue tracker](https://github.com/hcatlin/libsass/issues). Install issues should be reported on the node-sass [issue tracker](https://github.com/sass/node-sass/issues). Learn how [semver works](https://nodesource.com/blog/semver-tilde-and-caret) before opening a PR updating node-sass.*

This task uses [libsass](http://libsass.org), which is a Sass compiler in C++. It's a lot faster than the original Ruby compiler and [fully compatible](http://sass-compatibility.github.io/).

---

<p align="center"><b>🔥 Want to strengthen your core JavaScript skills and master ES6?</b><br>I would personally recommend this awesome <a href="https://ES6.io/friend/AWESOME">ES6 course</a> by Wes Bos.</p>

---


## Install

```
$ npm install --save-dev grunt-sass
```


## Usage

```js
require('load-grunt-tasks')(grunt); // npm install --save-dev load-grunt-tasks

grunt.initConfig({
	sass: {
		options: {
			sourceMap: true
		},
		dist: {
			files: {
				'main.css': 'main.scss'
			}
		}
	}
});

grunt.registerTask('default', ['sass']);
```

Files starting with `_` are ignored to match the expected [Sass partial behaviour](http://sass-lang.com/documentation/file.SASS_REFERENCE.html#partials).


## Options

See the `node-sass` [options](https://github.com/sass/node-sass#options), except for `file`, `outFile`, `success`, `error`.

The default value for the `precision` option is `10`, so you don't have to change it when using Bootstrap.


## License

MIT © [Sindre Sorhus](https://sindresorhus.com)
