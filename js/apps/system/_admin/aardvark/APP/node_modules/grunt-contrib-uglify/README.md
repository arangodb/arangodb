# grunt-contrib-uglify v0.7.0 [![Build Status: Linux](https://travis-ci.org/gruntjs/grunt-contrib-uglify.svg?branch=master)](https://travis-ci.org/gruntjs/grunt-contrib-uglify) [![Build Status: Windows](https://ci.appveyor.com/api/projects/status/ybtf5vbvtenii561/branch/master?svg=true)](https://ci.appveyor.com/project/gruntjs/grunt-contrib-uglify/branch/master)

> Minify files with UglifyJS.



## Getting Started
This plugin requires Grunt `~0.4.0`

If you haven't used [Grunt](http://gruntjs.com/) before, be sure to check out the [Getting Started](http://gruntjs.com/getting-started) guide, as it explains how to create a [Gruntfile](http://gruntjs.com/sample-gruntfile) as well as install and use Grunt plugins. Once you're familiar with that process, you may install this plugin with this command:

```shell
npm install grunt-contrib-uglify --save-dev
```

Once the plugin has been installed, it may be enabled inside your Gruntfile with this line of JavaScript:

```js
grunt.loadNpmTasks('grunt-contrib-uglify');
```




## Uglify task
_Run this task with the `grunt uglify` command._

Task targets, files and options may be specified according to the grunt [Configuring tasks](http://gruntjs.com/configuring-tasks) guide.

### Migrating from 2.x to 3.x

Version `3.x` introduced changes to configuring source maps. Accordingly, if you don't use the source map options you should be able to upgrade seamlessly. If you do use source maps, see below.

#### Removed options

`sourceMappingURL` - This is calculated automatically now
`sourceMapPrefix` - No longer necessary for the above reason

#### Changed options

`sourceMap` - Only accepts a `Boolean` value. Generates a map with a default name for you
`sourceMapRoot` - The location of your sources is now calculated for you when `sourceMap` is set to `true` but you can set manual source root if needed

#### New options

`sourceMapName` - Accepts a string or function to change the location or name of your map
`sourceMapIncludeSources` - Embed the content of your source files directly into the map
`expression` - Accepts a `Boolean` value. Parse a single expression (JSON or single functions)

### Options

This task primarily delegates to [UglifyJS2][], so please consider the [UglifyJS documentation][] as required reading for advanced configuration.

[UglifyJS2]: https://github.com/mishoo/UglifyJS2
[UglifyJS documentation]: http://lisperator.net/uglifyjs/

#### mangle
Type: `Boolean` `Object`  
Default: `{}`

Turn on or off mangling with default options. If an `Object` is specified, it is passed directly to `ast.mangle_names()` *and* `ast.compute_char_frequency()` (mimicking command line behavior).

#### compress
Type: `Boolean` `Object`  
Default: `{}`

Turn on or off source compression with default options. If an `Object` is specified, it is passed as options to `UglifyJS.Compressor()`.

#### beautify
Type: `Boolean` `Object`  
Default: `false`

Turns on beautification of the generated source code. An `Object` will be merged and passed with the options sent to `UglifyJS.OutputStream()`

###### expression
Type: `Boolean`
Default: `false`

Parse a single expression, rather than a program (for parsing JSON)

#### report
Choices: `'min'`, `'gzip'`  
Default: `'min'`

Either report only minification result or report minification and gzip results.
This is useful to see exactly how well clean-css is performing but using `'gzip'` will make the task take 5-10x longer to complete. [Example output](https://github.com/sindresorhus/maxmin#readme).

#### sourceMap
Type: `Boolean`  
Default: `false`

If `true`, a source map file will be generated in the same directory as the `dest` file. By default it will have the same basename as the `dest` file, but with a `.map` extension.

#### sourceMapName
Type: `String`  `Function`  
Default: `undefined`

To customize the name or location of the generated source map, pass a string to indicate where to write the source map to. If a function is provided, the uglify destination is passed as the argument and the return value will be used as the file name.

#### sourceMapIn
Type: `String`  `Function`  
Default: `undefined`

The location of an input source map from an earlier compilation, e.g. from CoffeeScript. If a function is provided, the
uglify source is passed as the argument and the return value will be used as the sourceMap name. This only makes sense
when there's one source file.

#### sourceMapIncludeSources
Type: `Boolean`
Default: `false`

Pass this flag if you want to include the content of source files in the source map as sourcesContent property.

###### sourceMapRoot
Type: `String`
Default: `undefined`

With this option you can customize root URL that browser will use when looking for sources.

If the sources are not absolute URLs after prepending of the `sourceMapRoot`, the sources are resolved relative to the source map.

###### enclose
Type: `Object`  
Default: `undefined`

Wrap all of the code in a closure with a configurable arguments/parameters list.
Each key-value pair in the `enclose` object is effectively an argument-parameter pair.

#### wrap
Type: `String`  
Default: `undefined`

Wrap all of the code in a closure, an easy way to make sure nothing is leaking.
For variables that need to be public `exports` and `global` variables are made available.
The value of wrap is the global variable exports will be available as.

#### maxLineLen
Type: `Number`
Default: `32000`

Limit the line length in symbols. Pass maxLineLen = 0 to disable this safety feature.

#### ASCIIOnly
Type: `Boolean`
Default: `false`

Enables to encode non-ASCII characters as \uXXXX.

#### exportAll
Type: `Boolean`  
Default: `false`

When using `wrap` this will make all global functions and variables available via the export variable.

#### preserveComments
Type: `Boolean` `String` `Function`  
Default: `undefined`  
Options: `false` `'all'` `'some'`

Turn on preservation of comments.

- `false` will strip all comments
- `'all'` will preserve all comments in code blocks that have not been squashed or dropped
- `'some'` will preserve all comments that start with a bang (`!`) or include a closure compiler style directive (`@preserve` `@license` `@cc_on`)
- `Function` specify your own comment preservation function. You will be passed the current node and the current comment and are expected to return either `true` or `false`

#### banner
Type: `String`  
Default: empty string

This string will be prepended to the minified output.  Template strings (e.g. `<%= config.value %>` will be expanded automatically.

#### footer
Type: `String`  
Default: empty string

This string will be appended to the minified output.  Template strings (e.g. `<%= config.value %>` will be expanded automatically.

### Usage examples

#### Basic compression

This configuration will compress and mangle the input files using the default options.

```js
// Project configuration.
grunt.initConfig({
  uglify: {
    my_target: {
      files: {
        'dest/output.min.js': ['src/input1.js', 'src/input2.js']
      }
    }
  }
});
```

#### No mangling

Specify `mangle: false` to prevent changes to your variable and function names.

```js
// Project configuration.
grunt.initConfig({
  uglify: {
    options: {
      mangle: false
    },
    my_target: {
      files: {
        'dest/output.min.js': ['src/input.js']
      }
    }
  }
});
```

#### Reserved identifiers

You can specify identifiers to leave untouched with an `except` array in the `mangle` options.

```js
// Project configuration.
grunt.initConfig({
  uglify: {
    options: {
      mangle: {
        except: ['jQuery', 'Backbone']
      }
    },
    my_target: {
      files: {
        'dest/output.min.js': ['src/input.js']
      }
    }
  }
});
```

#### Source maps

Generate a source map by setting the `sourceMap` option to `true`. The generated
source map will be in the same directory as the destination file. Its name will be the
basename of the destination file with a `.map` extension. Override these
defaults with the `sourceMapName` attribute.

```js
// Project configuration.
grunt.initConfig({
  uglify: {
    my_target: {
      options: {
        sourceMap: true,
        sourceMapName: 'path/to/sourcemap.map'
      },
      files: {
        'dest/output.min.js': ['src/input.js']
      }
    }
  }
});
```

#### Advanced source maps

Set the `sourceMapIncludeSources` option to `true` to embed your sources directly into the map. To include
a source map from a previous compilation pass it as the value of the `sourceMapIn` option.

```js
// Project configuration.
grunt.initConfig({
  uglify: {
    my_target: {
      options: {
        sourceMap: true,
        sourceMapIncludeSources: true,
        sourceMapIn: 'example/coffeescript-sourcemap.js', // input sourcemap from a previous compilation
      },
      files: {
        'dest/output.min.js': ['src/input.js'],
      },
    },
  },
});
```

Refer to the [UglifyJS SourceMap Documentation](http://lisperator.net/uglifyjs/codegen#source-map) for more information.


#### Turn off console warnings

Specify `drop_console: true` as part of the `compress` options to discard calls to `console.*` functions.
This will supress warning messages in the console.

```js
// Project configuration.
grunt.initConfig({
  uglify: {
    options: {
      compress: {
        drop_console: true
      }
    },
    my_target: {
      files: {
        'dest/output.min.js': ['src/input.js']
      }
    }
  }
});
```

#### Beautify

Specify `beautify: true` to beautify your code for debugging/troubleshooting purposes.
Pass an object to manually configure any other output options passed directly to `UglifyJS.OutputStream()`.

See [UglifyJS Codegen documentation](http://lisperator.net/uglifyjs/codegen) for more information.

_Note that manual configuration will require you to explicitly set `beautify: true` if you want traditional, beautified output._

```js
// Project configuration.
grunt.initConfig({
  uglify: {
    my_target: {
      options: {
        beautify: true
      },
      files: {
        'dest/output.min.js': ['src/input.js']
      }
    },
    my_advanced_target: {
      options: {
        beautify: {
          width: 80,
          beautify: true
        }
      },
      files: {
        'dest/output.min.js': ['src/input.js']
      }
    }
  }
});
```

#### Banner comments

In this example, running `grunt uglify:my_target` will prepend a banner created by interpolating the `banner` template string with the config object. Here, those properties are the values imported from the `package.json` file (which are available via the `pkg` config property) plus today's date.

_Note: you don't have to use an external JSON file. It's also valid to create the `pkg` object inline in the config. That being said, if you already have a JSON file, you might as well reference it._

```js
// Project configuration.
grunt.initConfig({
  pkg: grunt.file.readJSON('package.json'),
  uglify: {
    options: {
      banner: '/*! <%= pkg.name %> - v<%= pkg.version %> - ' +
        '<%= grunt.template.today("yyyy-mm-dd") %> */'
    },
    my_target: {
      files: {
        'dest/output.min.js': ['src/input.js']
      }
    }
  }
});
```

#### Conditional compilation

You can also enable UglifyJS conditional compilation. This is commonly used to remove debug code blocks for production builds.

See [UglifyJS global definitions documentation](http://lisperator.net/uglifyjs/compress#global-defs) for more information.

```js
// Project configuration.
grunt.initConfig({
  uglify: {
    options: {
      compress: {
        global_defs: {
          "DEBUG": false
        },
        dead_code: true
      }
    },
    my_target: {
      files: {
        'dest/output.min.js': ['src/input.js']
      }
    }
  }
});
```
#### Compiling all files in a folder dynamically

This configuration will compress and mangle the files dynamically.

```js
// Project configuration.
grunt.initConfig({
  uglify: {
    my_target: {
      files: [{
          expand: true,
          cwd: 'src/js',
          src: '**/*.js',
          dest: 'dest/js'
      }]
    }
  }
});
```


## Release History

 * 2014-12-23   v0.7.0   Adds sourceMapRoot options. Updates readme descriptions. Removes reference to cleancss.
 * 2014-09-17   v0.6.0   Output fixes. ASCIIOnly option. Other fixes.
 * 2014-07-25   v0.5.1   Chalk updates. Output updates.
 * 2014-03-01   v0.4.0   remove grunt-lib-contrib dependency and add more colors
 * 2014-02-27   v0.3.3   remove unnecessary calls to `grunt.template.process`
 * 2014-01-22   v0.3.2   fix handling of `sourceMapIncludeSources` option.
 * 2014-01-20   v0.3.1   fix relative path issue in sourcemaps
 * 2014-01-16   v0.3.0   refactor sourcemap support
 * 2013-11-09   v0.2.7   prepending banner if sourceMap option not set, addresses
 * 2013-11-08   v0.2.6   merged 45, 53, 85 (105 by way of duping 53) Added support for banners in uglified files with sourcemaps Updated docs
 * 2013-10-28   v0.2.5   Added warning for banners when using sourcemaps
 * 2013-09-02   v0.2.4   updated sourcemap format via /83
 * 2013-06-10   v0.2.3   added footer option
 * 2013-05-31   v0.2.2   Reverted /56 due to /58 until [chrome/239660](https://code.google.com/p/chromium/issues/detail?id=239660&q=sourcemappingurl&colspec=ID%20Pri%20M%20Iteration%20ReleaseBlock%20Cr%20Status%20Owner%20Summary%20OS%20Modified) [firefox/870361](https://bugzilla.mozilla.org/show_bug.cgi?id=870361) drop
 * 2013-05-22   v0.2.1   Bumped uglify to ~2.3.5 /55 /40 Changed sourcemappingUrl syntax /56 Disabled sorting of names for consistent mangling /44 Updated docs for sourceMapRoot /47 /25
 * 2013-03-14   v0.2.0   No longer report gzip results by default. Support `report` option.
 * 2013-01-30   v0.1.2   Added better error reporting Support for dynamic names of multiple sourcemaps
 * 2013-02-15   v0.1.1   First official release for Grunt 0.4.0.
 * 2013-01-18   v0.1.1rc6   Updating grunt/gruntplugin dependencies to rc6. Changing in-development grunt/gruntplugin dependency versions from tilde version ranges to specific versions.
 * 2013-01-09   v0.1.1rc5   Updating to work with grunt v0.4.0rc5. Switching back to this.files api.
 * 2012-11-28   v0.1.0   Work in progress, not yet officially released.

---

Task submitted by ["Cowboy" Ben Alman](http://benalman.com)

*This file was generated on Tue Dec 23 2014 16:18:53.*
