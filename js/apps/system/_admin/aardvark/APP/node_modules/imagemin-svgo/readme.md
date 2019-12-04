# imagemin-svgo [![Build Status](https://travis-ci.org/imagemin/imagemin-svgo.svg?branch=master)](https://travis-ci.org/imagemin/imagemin-svgo) [![Build status](https://ci.appveyor.com/api/projects/status/esa7m3u8bcol1mtr/branch/master?svg=true)](https://ci.appveyor.com/project/ShinnosukeWatanabe/imagemin-svgo/branch/master)


> [svgo](https://github.com/svg/svgo) imagemin plugin


## Install

```
$ npm install --save imagemin-svgo
```


## Usage

```js
const Imagemin = require('imagemin');
const imageminSvgo = require('imagemin-svgo');

new Imagemin()
	.src('images/*.svg')
	.dest('build/images')
	.use(imageminSvgo())
	.run();
```

You can also use this plugin with [gulp](http://gulpjs.com):

```js
const gulp = require('gulp');
const imageminSvgo = require('imagemin-svgo');

gulp.task('default', () => {
	return gulp.src('images/*.svg')
		.pipe(imageminSvgo()())
		.pipe(gulp.dest('build/images'));
});
```


## API

### imageminSvgo(options)

#### options

Type: `object`

Pass options to [svgo](https://github.com/svg/svgo#what-it-can-do).


## License

MIT © [imagemin](https://github.com/imagemin)
