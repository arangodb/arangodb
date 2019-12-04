# imagemin-gifsicle [![Build Status](http://img.shields.io/travis/imagemin/imagemin-gifsicle.svg?style=flat)](https://travis-ci.org/imagemin/imagemin-gifsicle) [![Build status](https://ci.appveyor.com/api/projects/status/51vfu1ntxwx7t949?svg=true)](https://ci.appveyor.com/project/ShinnosukeWatanabe/imagemin-gifsicle)

> gifsicle imagemin plugin


## Install

```
$ npm install --save imagemin-gifsicle
```


## Usage

```js
var Imagemin = require('imagemin');
var imageminGifsicle = require('imagemin-gifsicle');

new Imagemin()
	.src('images/*.gif')
	.dest('build/images')
	.use(imageminGifsicle({interlaced: true}))
	.run();
```

You can also use this plugin with [gulp](http://gulpjs.com):

```js
var gulp = require('gulp');
var imageminGifsicle = require('imagemin-gifsicle');

gulp.task('default', function () {
	return gulp.src('images/*.gif')
		.pipe(imageminGifsicle({interlaced: true})())
		.pipe(gulp.dest('build/images'));
});
```


## API

### imageminGifsicle(options)

### options.interlaced

Type: `boolean`  
Default: `false`

Interlace gif for progressive rendering.


## License

MIT © [imagemin](https://github.com/imagemin)
