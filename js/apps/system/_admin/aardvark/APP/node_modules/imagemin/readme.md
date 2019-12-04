# imagemin [![Build Status](https://img.shields.io/travis/imagemin/imagemin.svg)](https://travis-ci.org/imagemin/imagemin) [![Build status](https://ci.appveyor.com/api/projects/status/wlnem7wef63k4n1t?svg=true)](https://ci.appveyor.com/project/ShinnosukeWatanabe/imagemin)

> Minify images seamlessly


## Install

```
$ npm install --save imagemin
```


## Usage

```js
var Imagemin = require('imagemin');

new Imagemin()
	.src('images/*.{gif,jpg,png,svg}')
	.dest('build/images')
	.use(Imagemin.jpegtran({progressive: true}))
	.run(function (err, files) {
		console.log(files[0]);
		// => {path: 'build/images/foo.jpg', contents: <Buffer 89 50 4e ...>}
	});
```

You can use [gulp-rename](https://github.com/hparra/gulp-rename) to rename your files:

```js
var Imagemin = require('imagemin');
var rename = require('gulp-rename');

new Imagemin()
	.src('images/foo.png')
	.use(rename('bar.png'));
```


## API

### new Imagemin()

Creates a new `Imagemin` instance.

### .src(file)

Type: `array`, `buffer` or `string`

Set the files to be optimized. Takes a buffer, glob string or an array of glob strings 
as argument.

### .dest(folder)

Type: `string`

Set the destination folder to where your files will be written. If you don't set 
any destination no files will be written.

### .use(plugin)

Type: `function`

Add a `plugin` to the middleware stack.

### .run(callback)

Type: `function`

Optimize your files with the given settings.

#### callback(err, files)

The callback will return an array of vinyl files in `files`.


## Plugins

The following [plugins](https://www.npmjs.org/browse/keyword/imageminplugin) are bundled with imagemin:

* [gifsicle](#gifsicleoptions) — Compress GIF images.
* [jpegtran](#jpegtranoptions) — Compress JPG images.
* [optipng](#optipngoptions) — Compress PNG images losslessly.
* [svgo](#svgooptions) — Compress SVG images.

### .gifsicle(options)

Compress GIF images.

```js
var Imagemin = require('imagemin');

new Imagemin()
	.use(Imagemin.gifsicle({interlaced: true}));
```

### .jpegtran(options)

Compress JPG images.

```js
var Imagemin = require('imagemin');

new Imagemin()
	.use(Imagemin.jpegtran({progressive: true}));
```

### .optipng(options)

Lossless compression of PNG images.

```js
var Imagemin = require('imagemin');

new Imagemin()
	.use(Imagemin.optipng({optimizationLevel: 3}));
```

### .svgo(options)

Compress SVG images.

```js
var Imagemin = require('imagemin');

new Imagemin()
	.use(Imagemin.svgo());
```


## Related

- [imagemin-cli](https://github.com/imagemin/imagemin-cli) - CLI for this module
- [imagemin-app](https://github.com/imagemin/imagemin-app) - GUI app for this module.
- [gulp-imagemin](https://github.com/sindresorhus/gulp-imagemin) - Gulp plugin.
- [grunt-contrib-imagemin](https://github.com/gruntjs/grunt-contrib-imagemin) - Grunt plugin.


## License

MIT © [imagemin](https://github.com/imagemin)
