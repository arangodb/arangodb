# grunt-contrib-cssmin v1.0.2 [![Build Status: Linux](https://travis-ci.org/gruntjs/grunt-contrib-cssmin.svg?branch=master)](https://travis-ci.org/gruntjs/grunt-contrib-cssmin) [![Build Status: Windows](https://ci.appveyor.com/api/projects/status/ntgfqc3ppk533m84/branch/master?svg=true)](https://ci.appveyor.com/project/gruntjs/grunt-contrib-cssmin/branch/master)

> Minify CSS



## Getting Started

If you haven't used [Grunt](http://gruntjs.com/) before, be sure to check out the [Getting Started](http://gruntjs.com/getting-started) guide, as it explains how to create a [Gruntfile](http://gruntjs.com/sample-gruntfile) as well as install and use Grunt plugins. Once you're familiar with that process, you may install this plugin with this command:

```shell
npm install grunt-contrib-cssmin --save-dev
```

Once the plugin has been installed, it may be enabled inside your Gruntfile with this line of JavaScript:

```js
grunt.loadNpmTasks('grunt-contrib-cssmin');
```

**Issues with the output should be reported on the clean-css [issue tracker](https://github.com/GoalSmashers/clean-css/issues).**



## Cssmin task
_Run this task with the `grunt cssmin` command._


### Options

Options are passed to [clean-css](https://github.com/jakubpawlowicz/clean-css#how-to-use-clean-css-api). In addition this task defines some extra options:


#### report

Type: `string`  
Choices: `'min'`, `'gzip'`  
Default: `'min'`

Report minification result or both minification and gzip results.
This is useful to see exactly how well clean-css is performing but using `'gzip'` will make the task take 5-10x longer to complete. [Example output](https://github.com/sindresorhus/maxmin#readme).


#### sourceMap

Type: `boolean`  
Choices: `true`, `false`  
Default: `false`

Enable Source Maps.

### Usage

#### Combine two files into one output file

```js
cssmin: {
  options: {
    shorthandCompacting: false,
    roundingPrecision: -1
  },
  target: {
    files: {
      'output.css': ['foo.css', 'bar.css']
    }
  }
}
```

#### Minify all contents of a release directory and add a `.min.css` extension

```js
cssmin: {
  target: {
    files: [{
      expand: true,
      cwd: 'release/css',
      src: ['*.css', '!*.min.css'],
      dest: 'release/css',
      ext: '.min.css'
    }]
  }
}
```


## Release History

 * 2016-08-31   v1.0.2   Fix issues for node 6.
 * 2016-03-16   v1.0.1   Downgrade maxmin to support Node.js 0.10.
 * 2016-03-04   v1.0.0   Updated docs. Point main to task and removed peerDeps. Update clean-css to v3.4.1.
 * 2015-09-15   v0.14.0   Bump to cssmin ~3.4.0. Minor test fixes.
 * 2015-07-27   v0.13.0   Bump to cssmin ~3.3.0.
 * 2015-05-09   v0.12.3   Improve reporting of errors and warnings. Log out written files and the saved size. Fix absolute paths.
 * 2015-02-20   v0.12.2   Set the `rebase` option to `false` by default.
 * 2015-02-09   v0.12.0   Add `sourceMap` option.
 * 2014-12-24   v0.11.0   Bump `clean-css` to 3.0.1. Remove `banner` option.
 * 2014-06-11   v0.10.0   Update clean-css v2.2.0.
 * 2014-02-01   v0.9.0   Refactor. Remove grunt-lib-contrib dependency. Backwards-compatibly remove `false` choice from `report`.
 * 2014-02-14   v0.8.0   Update clean-css v2.1.0.
 * 2013-11-23   v0.7.0   Update clean-css v2.0.0.
 * 2013-09-14   v0.6.2   Support relative URLs via clean-css ~1.1.1.
 * 2013-05-25   v0.6.1   Support import in-lining via clean-css ~1.0.4.
 * 2013-04-05   v0.6.0   Update clean-css dependency to ~1.0.0.
 * 2013-03-14   v0.5.0   Support for `report` option (false by default).
 * 2013-03-10   v0.4.2   Add `banner` option. Support clean-css `keepSpecialComments`.
 * 2013-02-17   v0.4.1   Update clean-css dependency to ~0.10.0.
 * 2013-02-15   v0.4.0   First official release for Grunt 0.4.0.
 * 2013-01-23   v0.4.0rc7   Updating grunt/gruntplugin dependencies to rc7. Changing in-development grunt/gruntplugin dependency versions from tilde version ranges to specific versions.
 * 2013-01-09   v0.4.0rc5   Updating to work with grunt v0.4.0rc5. Switching to `this.files` API.
 * 2012-11-01   v0.3.2   Update clean-css dep.
 * 2012-10-12   v0.3.1   Rename grunt-contrib-lib dep to grunt-lib-contrib.
 * 2012-09-23   v0.3.0   Options no longer accepted from global config key.
 * 2012-09-10   v0.2.0   Refactored from grunt-contrib into individual repo.

---

Task submitted by [Tim Branyen](http://tbranyen.com/)

*This file was generated on Wed Aug 31 2016 23:09:56.*
