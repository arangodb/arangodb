# imagemin-jpegtran [![Build Status](https://travis-ci.org/imagemin/imagemin-jpegtran.svg?branch=master)](https://travis-ci.org/imagemin/imagemin-jpegtran) [![Build status](https://ci.appveyor.com/api/projects/status/rwf4by6qcbne1qet?svg=true)](https://ci.appveyor.com/project/ShinnosukeWatanabe/imagemin-jpegtran)

> jpegtran imagemin plugin


## Install

```
$ npm install --save imagemin-jpegtran
```


## Usage

```js
const Imagemin = require('imagemin');
const imageminJpegtran = require('imagemin-jpegtran');

new Imagemin()
	.src('images/*.jpg')
	.dest('build/images')
	.use(imageminJpegtran({progressive: true}))
	.run();
```

You can also use this plugin with [gulp](http://gulpjs.com):

```js
const gulp = require('gulp');
const imageminJpegtran = require('imagemin-jpegtran');

gulp.task('default', () => {
	return gulp.src('images/*.jpg')
		.pipe(imageminJpegtran({progressive: true})())
		.pipe(gulp.dest('build/images'));
});
```


## API

### imageminJpegtran(options)

#### options.progressive

Type: `boolean`  
Default: `false`

Lossless conversion to progressive.

#### options.arithmetic

Type: `boolean`  
Default: `false`

Use [arithmetic coding](http://en.wikipedia.org/wiki/Arithmetic_coding).


## License

MIT © [imagemin](https://github.com/imagemin)
