# grunt-contrib-compress v1.4.3 [![Build Status: Linux](https://travis-ci.org/gruntjs/grunt-contrib-compress.svg?branch=master)](https://travis-ci.org/gruntjs/grunt-contrib-compress) [![Build Status: Windows](https://ci.appveyor.com/api/projects/status/tiwbi1smm1j8aa5j/branch/master?svg=true)](https://ci.appveyor.com/project/gruntjs/grunt-contrib-compress/branch/master)

> Compress files and folders



## Getting Started

If you haven't used [Grunt](http://gruntjs.com/) before, be sure to check out the [Getting Started](http://gruntjs.com/getting-started) guide, as it explains how to create a [Gruntfile](http://gruntjs.com/sample-gruntfile) as well as install and use Grunt plugins. Once you're familiar with that process, you may install this plugin with this command:

```shell
npm install grunt-contrib-compress --save-dev
```

Once the plugin has been installed, it may be enabled inside your Gruntfile with this line of JavaScript:

```js
grunt.loadNpmTasks('grunt-contrib-compress');
```

*This plugin was designed to work with Grunt >= 0.4.x. If you're still using grunt v0.3.x it's strongly recommended that [you upgrade](http://gruntjs.com/upgrading-from-0.3-to-0.4), but in case you can't please use [v0.3.2](https://github.com/gruntjs/grunt-contrib-compress/tree/grunt-0.3-stable).*



## Compress task
_Run this task with the `grunt compress` command._

Task targets, files and options may be specified according to the grunt [Configuring tasks](http://gruntjs.com/configuring-tasks) guide.

Node Libraries Used:
[archiver](https://github.com/ctalkington/node-archiver) (for zip/tar)
[zlib](https://nodejs.org/api/zlib.html#zlib_options) (for gzip).
### Options

#### archive
Type: `String` or `Function`  
Modes: `zip` `tar`

This is used to define where to output the archive. Each target can only have one output file.
If the type is a Function it must return a String.

*This option is only appropriate for many-files-to-one compression modes like zip and tar.  For gzip for example, please use grunt's standard src/dest specifications.*

#### mode
Type: `String`

This is used to define which mode to use, currently supports `gzip`, `deflate`, `deflateRaw`, `tar`, `tgz` (tar gzip),`zip` and `brotli`.

Automatically detected per `dest:src` pair, but can be overridden per target if desired.

#### level
Type: `Integer`  
Modes: `zip` `gzip`  
Default: `1`

Sets the level of archive compression.

#### brotli
Configure brotli compression settings:

Type: `Object`  
Default:
```js
{
  mode: 0,
  quality: 11,
  lgwin: 22,
  lgblock: 0
}
```

##### mode
Type: `Integer`
* `0`: generic mode
* `1`: text mode
* `2`: font mode

Default: `0`

##### quality
Controls the compression-speed vs compression-density tradeoffs. The higher the quality, the slower the compression. Range is 0 to 11.

Type: `Integer`  
Default: `11`

##### lgwin
Base 2 logarithm of the sliding window size. Range is 10 to 24.

Type: `Integer`  
Default: `22`

##### lgblock
Base 2 logarithm of the maximum input block size. Range is 16 to 24. If set to 0, the value will be set based on the quality.  

Type: `Integer`  
Default: `0`

#### pretty
Type: `Boolean`  
Default: `false`

Pretty print file sizes when logging.

#### createEmptyArchive
Type: `Boolean`  
Default: `true`

This can be used when you don't want to get an empty archive as a result, if there are no files at the specified paths.

It may be useful, if you don't clearly know if files exist and you don't need an empty archive as a result.

### File Data

The following additional keys may be passed as part of a `dest:src` pair when using an Archiver-backed format.
All keys can be defined as a `Function` that receives the file name and returns in the type specified below.

#### date
Type: `Date`  
Modes: `zip` `tar` `tgz`

Sets the file date.

#### mode
Type: `Integer`  
Modes: `zip` `tar` `tgz`

Sets the file permissions.

#### store
Type: `Boolean`  
Default: `false`

If true, file contents will be archived without compression.

#### comment
Type: `String`  
Modes: `zip`

Sets the file comment.

#### gid
Type: `Integer`  
Modes: `tar` `tgz`

Sets the group of the file in the archive

#### uid
Type: `Integer`  
Modes: `tar` `tgz`

Sets the user of the file in the archive

### Usage Examples

```js
// make a zipfile
compress: {
  main: {
    options: {
      archive: 'archive.zip'
    },
    files: [
      {src: ['path/*'], dest: 'internal_folder/', filter: 'isFile'}, // includes files in path
      {src: ['path/**'], dest: 'internal_folder2/'}, // includes files in path and its subdirs
      {expand: true, cwd: 'path/', src: ['**'], dest: 'internal_folder3/'}, // makes all src relative to cwd
      {flatten: true, src: ['path/**'], dest: 'internal_folder4/', filter: 'isFile'} // flattens results to a single level
    ]
  }
}
```

```js
// gzip assets 1-to-1 for production
compress: {
  main: {
    options: {
      mode: 'gzip'
    },
    expand: true,
    cwd: 'assets/',
    src: ['**/*'],
    dest: 'public/'
  }
}
```

```js
// compress a file to a different location than its source
// example compresses path/the_file to /the_file inside archive.zip
compress: {
  main: {
    options: {
      archive: 'archive.zip'
    },
    files: [{
      expand: true,
      cwd: 'path/',
      src: ['the_file'],
      dest: '/'
    }]
  }
},
```

```js
// use custom extension for the output file
compress: {
  main: {
    options: {
      mode: 'gzip'
    },
    // Each of the files in the src/ folder will be output to
    // the dist/ folder each with the extension .gz.js
    files: [{
      expand: true,
      src: ['src/*.js'],
      dest: 'dist/',
      ext: '.gz.js'
    }]
  }
}

```
```js
// use a function to return the output file
compress: {
  main: {
    options: {
      archive: function () {
        // The global value git.tag is set by another task
        return git.tag + '.zip'
      }
    },
    files: [{
      expand: true,
      src: ['src/*.js'],
      dest: 'dist/'
    }]
  }
}
```

```js
// brotlify assets 1-to-1 for production using default options
compress: {
  main: {
    options: {
      mode: 'brotli'
    },
    expand: true,
    cwd: 'assets/',
    src: ['**/*.js'],
    dest: 'public/',
    extDot: 'last',
    ext: '.js.br'
  }
}
```

```js
// brotlify assets 1-to-1 for production specifying text mode
// and using default options otherwise
compress: {
  main: {
    options: {
      mode: 'brotli',
      brotli: {
        mode: 1
      }
    },
    expand: true,
    cwd: 'assets/',
    src: ['**/*.js'],
    dest: 'public/',
    extDot: 'last',
    ext: '.js.br'
  }
}
```


## Release History

 * 2017-05-20   v1.4.3   Update pretty-bytes to v4.0.2. Add option to not to create empty archive.
 * 2017-05-20   v1.4.2   Update archiver to v1.3.0.
 * 2017-01-20   v1.4.1   Make brotli support optional.
 * 2017-01-18   v1.4.0   Add support for brotli.
 * 2016-05-24   v1.3.0   Update archiver to v1.0. Fix node 6 support.
 * 2016-03-24   v1.2.0   Dependency update.
 * 2016-03-08   v1.1.1   Fix verbose output.
 * 2016-03-04   v1.1.0   Add ability to replace file in the same location.
 * 2016-02-15   v1.0.0   Update archiver, chalk and pretty-bytes.
 * 2015-10-23   v0.14.0   Change to verbose output. Dependency updates archiver 0.16.
 * 2014-12-25   v0.13.0   Update archiver to v0.13. Small fixes.
 * 2014-09-23   v0.12.0   Output update. Prevent zipping dot files and allow for forcing through `fail.warn` within loop.
 * 2014-08-26   v0.11.0   Update archiver to v0.11.0.
 * 2014-07-14   v0.10.0   Don't apply extensions automatically (use `ext` or `rename`).
 * 2014-05-20   v0.9.1   Allow directories to pass-through to archiver via filter.
 * 2014-05-20   v0.9.0   Update archiver to v0.9.0.
 * 2014-04-09   v0.8.0   Update archiver to v0.8.0.
 * 2014-02-17   v0.7.0   Update archiver to v0.6.0.
 * 2014-01-12   v0.6.0   Update archiver to v0.5.0.
 * 2013-11-27   v0.5.3   Allow archive option to be a function.
 * 2013-06-03   v0.5.2   Allow custom extensions using the `ext` option.
 * 2013-05-28   v0.5.1   Avoid gzip on folders.
 * 2013-04-23   v0.5.0   Add support for `deflate` and `deflateRaw`.
 * 2013-04-15   v0.4.10   Fix issue where task finished before all data was compressed.
 * 2013-04-09   v0.4.9   Bump Archiver version.
 * 2013-04-07   v0.4.8   Open streams lazily to avoid too many open files.
 * 2013-04-01   v0.4.7   Pipe gzip to fix gzip issues. Add tests that undo compressed files to test.
 * 2013-03-25   v0.4.6   Fix Node.js v0.8 compatibility issue with gzip.
 * 2013-03-20   v0.4.5   Update to archiver 0.4.1 Fix issue with gzip failing intermittently.
 * 2013-03-19   v0.4.4   Fixes for Node.js v0.10. Explicitly call `grunt.file` methods with `map` and `filter`.
 * 2013-03-14   v0.4.3   Fix for gzip; continue iteration on returning early.
 * 2013-03-13   v0.4.2   Refactor task like other contrib tasks. Fix gzip of multiple files. Remove unused dependencies.
 * 2013-02-22   v0.4.1   Pretty print compressed sizes. Logging each addition to a compressed file now only happens in verbose mode.
 * 2013-02-15   v0.4.0   First official release for Grunt 0.4.0.
 * 2013-01-23   v0.4.0rc7   Updating grunt/gruntplugin dependencies to rc7. Changing in-development grunt/gruntplugin dependency versions from tilde version ranges to specific versions.
 * 2013-01-14   v0.4.0rc5   Updating to work with grunt v0.4.0rc5. Conversion to grunt v0.4 conventions. Replace `basePath` with `cwd`.
 * 2012-10-12   v0.3.2   Rename grunt-contrib-lib dep to grunt-lib-contrib.
 * 2012-10-09   v0.3.1   Replace zipstream package with archiver.
 * 2012-09-24   v0.3.0   General cleanup. Options no longer accepted from global config key.
 * 2012-09-18   v0.2.2   Test refactoring. No valid source check. Automatic mode detection.
 * 2012-09-10   v0.2.0   Refactored from grunt-contrib into individual repo.

---

Task submitted by [Chris Talkington](http://christalkington.com/)

*This file was generated on Sat May 20 2017 14:05:16.*
