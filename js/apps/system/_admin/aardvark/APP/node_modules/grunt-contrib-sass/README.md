# grunt-contrib-sass v1.0.0 [![Build Status: Linux](https://travis-ci.org/gruntjs/grunt-contrib-sass.svg?branch=master)](https://travis-ci.org/gruntjs/grunt-contrib-sass)

> Compile Sass to CSS



## Getting Started

If you haven't used [Grunt](http://gruntjs.com/) before, be sure to check out the [Getting Started](http://gruntjs.com/getting-started) guide, as it explains how to create a [Gruntfile](http://gruntjs.com/sample-gruntfile) as well as install and use Grunt plugins. Once you're familiar with that process, you may install this plugin with this command:

```shell
npm install grunt-contrib-sass --save-dev
```

Once the plugin has been installed, it may be enabled inside your Gruntfile with this line of JavaScript:

```js
grunt.loadNpmTasks('grunt-contrib-sass');
```




## Sass task
_Run this task with the `grunt sass` command._

[Sass](http://sass-lang.com) is a preprocessor that adds nested rules, variables, mixins and functions, selector inheritance, and more to CSS. Sass files compile into well-formatted, standard CSS to use in your site or application.

This task requires you to have [Ruby](http://www.ruby-lang.org/en/downloads/) and [Sass](http://sass-lang.com/download.html) installed. If you're on OS X or Linux you probably already have Ruby installed; test with `ruby -v` in your terminal. When you've confirmed you have Ruby installed, run `gem install sass` to install Sass.

Note: Files that begin with "_" are ignored even if they match the globbing pattern. This is done to match the expected [Sass partial behaviour](http://sass-lang.com/documentation/file.SASS_REFERENCE.html#partials).

### Options


#### sourcemap

Type: `String`  
Default: `auto`

Values:

- `auto` - relative paths where possible, file URIs elsewhere
- `file` - always absolute file URIs
- `inline` - include the source text in the sourcemap
- `none`- no sourcemaps

**Requires Sass 3.4.0, which can be installed with `gem install sass`**


#### trace

Type: `Boolean`  
Default: `false`

Show a full traceback on error.


#### unixNewlines

Type: `Boolean`  
Default: `false` on Windows, otherwise `true`

Force Unix newlines in written files.


#### check

Type: `Boolean`  
Default: `false`

Just check the Sass syntax, does not evaluate and write the output.


#### style

Type: `String`  
Default: `nested`

Output style. Can be `nested`, `compact`, `compressed`, `expanded`.


#### precision

Type: `Number`  
Default: `5`

How many digits of precision to use when outputting decimal numbers.


#### quiet

Type: `Boolean`  
Default: `false`

Silence warnings and status messages during compilation.


#### compass

Type: `Boolean`  
Default: `false`

Make Compass imports available and load project configuration (`config.rb` located close to the `Gruntfile.js`).


#### debugInfo

Type: `Boolean`  
Default: `false`

Emit extra information in the generated CSS that can be used by the FireSass Firebug plugin.


#### lineNumbers

Type: `Boolean`  
Default: `false`

Emit comments in the generated CSS indicating the corresponding source line.


#### loadPath

Type: `String|Array`

Add a (or multiple) Sass import path.


#### require

Type: `String|Array`

Require a (or multiple) Ruby library before running Sass.


#### cacheLocation

Type: `String`  
Default: `.sass-cache`

The path to put cached Sass files.


#### noCache

Type: `Boolean`  
Default: `false`

Don't cache to sassc files.


#### bundleExec

Type: `Boolean`  
Default: `false`

Run `sass` with [bundle exec](http://gembundler.com/man/bundle-exec.1.html): `bundle exec sass`.


#### update

Type: `Boolean`  
Default: `false`

Only compile changed files.

### Examples

#### Example config

```js
grunt.initConfig({
  sass: {                              // Task
    dist: {                            // Target
      options: {                       // Target options
        style: 'expanded'
      },
      files: {                         // Dictionary of files
        'main.css': 'main.scss',       // 'destination': 'source'
        'widgets.css': 'widgets.scss'
      }
    }
  }
});

grunt.loadNpmTasks('grunt-contrib-sass');

grunt.registerTask('default', ['sass']);
```

#### Compile

```js
grunt.initConfig({
  sass: {
    dist: {
      files: {
        'main.css': 'main.scss'
      }
    }
  }
});
```

#### Concat and compile

Instead of concatenating the files, just `@import` them into another `.sass` file eg. `main.scss`.


#### Compile multiple files

You can specify multiple `destination: source` items in `files`.

```js
grunt.initConfig({
  sass: {
    dist: {
      files: {
        'main.css': 'main.scss',
        'widgets.css': 'widgets.scss'
      }
    }
  }
});
```

#### Compile files in a directory

Instead of naming all files you want to compile, you can use the `expand` property allowing you to specify a directory. More information available in the [grunt docs](http://gruntjs.com/configuring-tasks) - `Building the files object dynamically`.

```js
grunt.initConfig({
  sass: {
    dist: {
      files: [{
        expand: true,
        cwd: 'styles',
        src: ['*.scss'],
        dest: '../public',
        ext: '.css'
      }]
    }
  }
});
```


## Release History

 * 2016-03-04   v1.0.0   bump `chalk`. update to docs and project structure.
 * 2015-02-06   v0.9.0   Remove `banner` option. Allow using `--force` to ignore compile errors. Increase concurrency count from `2` to `4`. Improve Windows support.
 * 2014-08-24   v0.8.1   Fix `check` option.
 * 2014-08-21   v0.8.0   Support Sass 3.4 Source Map option. Add `update` option.
 * 2014-08-09   v0.7.4   Fix bundleExec option. Fix `os.cpus()` issue. Log `sass` command when `--verbose` flag is set.
 * 2014-03-06   v0.7.3   Only create empty dest files when they don't already exist.
 * 2014-02-02   v0.7.2   Fix error reporting when Sass is not available.
 * 2014-01-28   v0.7.1   Fix regression of Bundler support.
 * 2014-01-26   v0.7.0   Improve Windows support.
 * 2013-12-10   v0.6.0   Ignore files where filename have leading underscore.
 * 2013-08-21   v0.5.0   Add banner option.
 * 2013-07-06   v0.4.1   Use file.orig.src if file.src does not exist and return early to avoid passing non-existent files to sass binary.
 * 2013-06-30   v0.4.0   Rewrite task to be able to support Source Maps. Compile Sass files in parallel for better performance.
 * 2013-03-26   v0.3.0   Add support for `bundle exec`. Make sure `.css` files are compiled with SCSS.
 * 2013-02-15   v0.2.2   First official release for Grunt 0.4.0.
 * 2013-01-25   v0.2.2rc7   Updating grunt/gruntplugin dependencies to rc7. Changing in-development grunt/gruntplugin dependency versions from tilde version ranges to specific versions.
 * 2013-01-09   v0.2.2rc5   Updating to work with grunt v0.4.0rc5. Switching to this.files api. Add separator option.
 * 2012-11-05   v0.2.0   Grunt 0.4 compatibility. Improve error message when Sass binary couldn't be found
 * 2012-10-12   v0.1.3   Rename grunt-contrib-lib dep to grunt-lib-contrib.
 * 2012-10-08   v0.1.2   Fix regression for darwin.
 * 2012-10-05   v0.1.1   Windows support.
 * 2012-09-24   v0.1.0   Initial release.

---

Task submitted by [Sindre Sorhus](https://github.com/sindresorhus)

*This file was generated on Fri Mar 04 2016 15:57:01.*
