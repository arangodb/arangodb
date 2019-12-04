# grunt-processhtml

[![NPM version](https://img.shields.io/npm/v/grunt-processhtml.svg)](https://www.npmjs.com/package/grunt-processhtml)
[![Build Status](https://api.travis-ci.org/dciccale/grunt-processhtml.svg)](https://travis-ci.org/dciccale/grunt-processhtml)
[![Mentions](https://mentions-badge.com/dciccale/grunt-processhtml.svg)](https://mentions-badge.com/dciccale/grunt-processhtml)
[![NPM downloads](https://img.shields.io/npm/dm/grunt-processhtml.svg)](https://www.npmjs.com/package/grunt-processhtml)

[![Join the chat at https://gitter.im/dciccale/grunt-processhtml](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/dciccale/grunt-processhtml?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

> Process html files at build time to modify them depending on the release environment

### Looking for the stand-alone version?

Use [node-htmlprocessor](https://github.com/dciccale/node-htmlprocessor)

## Getting Started
This plugin requires Grunt `^1.0.1`

If you haven't used [Grunt](http://gruntjs.com/) before, be sure to check out the [Getting Started](http://gruntjs.com/getting-started) guide, as it explains how to create a [Gruntfile](http://gruntjs.com/sample-gruntfile) as well as install and use Grunt plugins. Once you're familiar with that process, you may install this plugin with this command:

```shell
npm install grunt-processhtml --save-dev
```

Once the plugin has been installed, it may be enabled inside your Gruntfile with this line of JavaScript:

```js
grunt.loadNpmTasks('grunt-processhtml');
```

## The "processhtml" task

Process `html` files with special comments:

```html
<!-- build:<type>[:target] [inline] [value] -->
...
<!-- /build -->
```

##### type
This is required.

Types: `js`, `css`, `remove`, `template` or `include`. or any html attribute if written like this: `[href]`, `[src]`, etc.

##### target
This is optional.

Is the target name of your grunt task, for example: `dist`. Is supported for all types, so you can always specify the target if needed.

You can pass multiple comma-separated targets, e.g. `<!-- build:remove:dist,dev,prod -->` and block will be parsed for each.

##### inline
This modifier can be used with `js` and `css` types.

If used styles or scripts will be included in the output html file.

##### value
Required for types: `js`, `css`, `include` and `[attr]`.

Optional for types: `remove`, `template` and `js`, `css` types with `inline` modifier.

Could be a file name: `script.min.js` or a path if an attribute like `[src]` is specified to keep the original file name intact but replace its path.

### Simple examples

##### `build:js[:targets] [inline] <value>`

Replace many script tags into one.

`[:targets]` Optional build targets.

`inline` Optional modifier.

`<value>` Required value: A file path.

```html
<!-- build:js app.min.js -->
<script src="my/lib/path/lib.js"></script>
<script src="my/deep/development/path/script.js"></script>
<!-- /build -->

<!-- changed to -->
<script src="app.min.js"></script>
```

You can embed your javascript:

```html
<!-- build:js inline app.min.js -->
<script src="my/lib/path/lib.js"></script>
<script src="my/deep/development/path/script.js"></script>
<!-- /build -->

<!-- changed to -->
<script>
  // app.min.js code here
</script>
```

or

```html
<!-- build:js inline -->
<script src="my/lib/path/lib.js"></script>
<script src="my/deep/development/path/script.js"></script>
<!-- /build -->

<!-- changed to -->
<script>
  // my/lib/path/lib.js code here then...
  // my/deep/development/path/script.js code goes here
</script>
```

##### `build:css[:targets] [inline] <value>`

Replace many stylesheet link tags into one.

`[:targets]` Optional build targets.

`inline` Optional modifier.

`<value>` Required value: A file path.

```html
<!-- build:css style.min.css -->
<link rel="stylesheet" href="path/to/normalize.css">
<link rel="stylesheet" href="path/to/main.css">
<!-- /build -->

<!-- changed to -->
<link rel="stylesheet" href="style.min.css">
```

You can embed your styles like with `js` type above:

```html
<!-- build:css inline -->
<link rel="stylesheet" href="path/to/normalize.css">
<link rel="stylesheet" href="path/to/main.css">
<!-- /build -->

<!-- changed to -->
<style>
  /* path/to/normalize.css */
  /* path/to/main.css */
</style>
```

or

```html
<!-- build:css inline style.min.css -->
<link rel="stylesheet" href="path/to/normalize.css">
<link rel="stylesheet" href="path/to/main.css">
<!-- /build -->

<!-- changed to -->
<style>
  /* style.min.css */
</style>
```

##### `build:<[attr]>[:targets] <value>`

Change the value of an attribute. In most cases using `[src]` and `[href]` will be enough but it works with any html attribute.

`<[attr]>` Required html attribute, i.e. `[src]`, `[href]`.

`[:targets]` Optional build targets.

`<value>` Required value: A path, a file path or any string value

```html
<!-- If only a path is used, the original file name will remain -->

<!-- build:[src] js/ -->
<script src="my/lib/path/lib.js"></script>
<script src="my/deep/development/path/script.js"></script>
<!-- /build -->

<!-- changed the src attribute path -->
<script src="js/lib.js"></script>
<script src="js/script.js"></script>

<!-- build:[href] img/ -->
<link rel="apple-touch-icon-precomposed" href="skins/demo/img/icon.png">
<link rel="apple-touch-icon-precomposed" href="skins/demo/img/icon-72x72.png" sizes="72x72">
<!-- /build -->

<!-- changed the href attribute path -->
<link rel="apple-touch-icon-precomposed" href="img/icon.png">
<link rel="apple-touch-icon-precomposed" href="img/icon-72x72.png" sizes="72x72">

<!-- build:[class]:dist production -->
<html class="debug_mode">
<!-- /build -->

<!-- this will change the class to 'production' only when de 'dist' build is executed -->
<html class="production">

```

##### `build:include[:targets] <value>`

Include an external file.

`[:targets]` Optional build targets.

`<value>` Required value: A file path.

```html
<!-- build:include header.html -->
This will be replaced by the content of header.html
<!-- /build -->

<!-- build:include:dev dev/content.html -->
This will be replaced by the content of dev/content.html
<!-- /build -->

<!-- build:include:dist dist/content.html -->
This will be replaced by the content of dist/content.html
<!-- /build -->
```

##### `build:template[:targets]`

Process a template block with a data object inside [options.data](#optionsdata).

`[:targets]` Optional build targets.


```html
<!-- build:template
<p>Hello, <%= name %></p>
/build -->

<!--
notice that the template block is commented
to prevent breaking the html file and keeping it functional
-->
```

##### `build:remove[:targets]`

Remove a block.

`[:targets]` Optional build targets

```html
<!-- build:remove -->
<p>This will be removed when any processhtml target is done</p>
<!-- /build -->

<!-- build:remove:dist -->
<p>But this one only when doing processhtml:dist target</p>
<!-- /build -->
```

### Overview
In your project's Gruntfile, add a section named `processhtml` to the data object passed into `grunt.initConfig()`.

```js
grunt.initConfig({
  processhtml: {
    options: {
      // Task-specific options go here.
    },
    your_target: {
      // Target-specific file lists and/or options go here.
    },
  },
})
```

### Options

#### options.process
Type: `Boolean`
Default value: `false`

Process the entire `html` file through `grunt.template.process`, a default object with the build target will be passed to the
template in the form of `{environment: target}` where environment will be the build target of the grunt task.

*Important note: The `process` option is not needed if you don't want to process the entire html file. See the example
below to see that you can have templates blocks to be processed.*

If you do want to process the whole file as a template, it will be compiled after compiling the inside template blocks
if any.

#### options.environment
Type: `Object`
Default value: `target`

The environemnt variable will be available to use in the comments, it defaults to the task target.

#### options.data
Type: `Object`
Default value: `{}`

An object `data` that is passed to the `html` file used to compile all template blocks and the entire file if `process`
is true.

#### options.templateSettings
Type: `Object`
Default value: `null` (Will use default lodash template delimiters `<%` and `%>`)

Define the `templateSettings` option with lodash [templateSettings](http://lodash.com/docs#templateSettings) options to customize the
template syntax.

```javascript
templateSettings: {
  interpolate: /{{([\s\S]+?)}}/g // mustache
}
```

#### options.includeBase
Type: `String`
Default value: `null` (Will use the path of the including file)

Specify an optional path to look for include files. ie, `app/assets/includes/`

#### options.commentMarker
Type: `String`
Default value: `build`

Specify the word used to indicate the special begin/end comments.  This is useful if you want to use this plugin
in conjunction with other plugins that use a similar, conflicting `build:<type>` comment
(such as [grunt-usemin](https://github.com/yeoman/grunt-usemin)).

With `options.commentMarker` set to `process`, a typical comment would look like:

```html
<!-- process:<type>[:targets] [value] -->
...
<!-- /process -->
```

#### options.strip
Type: `Boolean`
Default value: `null`

Specifying `true` will strip comments which do not match the current target:

```javascript
strip: true
```

#### options.recursive
Type: `Boolean`
Default value: `false`

Recursively process files that are being included using `build:include`.

```javascript
recursive: true
```

#### options.customBlockTypes
Type: `Array`
Default value: `[]`

Define an array of `.js` files that define custom block types.

```javascript
customBlockTypes: ['custom-blocktype.js']
```

A custom block type example:

`custom-blocktype.js`

```js
'use strict';

module.exports = function (processor) {
  // This will allow to use this <!-- build:customBlock[:target] <value> --> syntax
  processor.registerBlockType('customBlock', function (content, block, blockLine, blockContent) {
    var title = '<h1>' + block.asset + '</h1>';
    var result = content.replace(blockLine, title);

    return result;
  });
};
```

`file.html`

```html
<!-- build:customBlock myValue -->
<p>This will be replaced with the result of the custom block above</p>
<!-- /build -->
```

The result will be

```html
<h1>myValue</h1>
```


### Usage Examples

#### Default Options
Set the task in your grunt file which is going to process the `index.html` file and save the output to
`dest/index.html`

```js
grunt.initConfig({
  processhtml: {
    options: {
      data: {
        message: 'Hello world!'
      }
    },
    dist: {
      files: {
        'dest/index.html': ['index.html']
      }
    }
  }
});
```

#### What will be processed?
Following the previous task configuration, the `index.html` could look like this:

```html
<!doctype html>
<title>title</title>

<!-- build:[href] img/ -->
<link rel="apple-touch-icon-precomposed" href="my/theme/img/apple-touch-icon-precomposed.png">
<link rel="apple-touch-icon-precomposed" href="my/theme/img/apple-touch-icon-72x72-precomposed.png" sizes="72x72">
<!-- /build -->

<!-- build:css style.min.css -->
<link rel="stylesheet" href="normalize.css">
<link rel="stylesheet" href="main.css">
<!-- /build -->

<!-- build:js app.min.js -->
<script src="js/libs/require.js" data-main="js/config.js"></script>
<!-- /build -->

<!-- build:include header.html -->
This will be replaced by the content of header.html
<!-- /build -->

<!-- build:template
<p><%= message %></p>
/build -->

<!-- build:remove -->
<p>This is the html file without being processed</p>
<!-- /build -->
```

After processing this file, the output will be:

```html
<!doctype html>
<title>title</title>

<link rel="apple-touch-icon-precomposed" href="img/apple-touch-icon-precomposed.png">
<link rel="apple-touch-icon-precomposed" href="img/apple-touch-icon-72x72-precomposed.png" sizes="72x72">

<link rel="stylesheet" href="style.min.css">

<script src="app.min.js"></script>

<h1>Content from header.html</h1>

<p>Hello world!</p>
```

#### Advanced example
In this example there are multiple targets where we can process the html file depending on which target is being run.

```js
grunt.initConfig({
  processhtml: {
    dev: {
      options: {
        data: {
          message: 'This is development environment'
        }
      },
      files: {
        'dev/index.html': ['index.html']
      }
    },
    dist: {
      options: {
        process: true,
        data: {
          title: 'My app',
          message: 'This is production distribution'
        }
      },
      files: {
        'dest/index.html': ['index.html']
      }
    },
    custom: {
      options: {
        templateSettings: {
          interpolate: /{{([\s\S]+?)}}/g // mustache
        },
        data: {
          message: 'This has custom template delimiters'
        }
      },
      files: {
        'custom/custom.html': ['custom.html']
      }
    }
  }
});
```

The `index.html` to be processed (the `custom.html` is below):

```html
<!doctype html>
<!-- notice that no special comment is used here, as process is true.
if you don't mind having <%= title %> as the title of your app
when not being processed; is a perfectly valid title string -->
<title><%= title %></title>

<!-- build:css style.min.css -->
<link rel="stylesheet" href="normalize.css">
<link rel="stylesheet" href="main.css">
<!-- /build -->

<!-- build:template
<p><%= message %></p>
/build -->

<!-- build:remove -->
<p>This is the html file without being processed</p>
<!-- /build -->

<!-- build:remove:dist -->
<script src="js/libs/require.js" data-main="js/config.js"></script>
<!-- /build -->

<!-- build:template
<% if (environment === 'dev') { %>
<script src="app.js"></script>
<% } else { %>
<script src="app.min.js"></script>
<% } %>
/build -->
```

The `custom.html` to be processed:

```html
<!doctype html>
<html>
  <head>
    <title>Custom template delimiters example</title>
  </head>

  <body>
    <!-- build:template
    {{ message }}
    /build -->
  </body>
</html>
```

## Contributing
In lieu of a formal styleguide, take care to maintain the existing coding style. Add unit tests for any new or changed functionality. Lint and test your code using [Grunt](http://gruntjs.com/).

## Release History
- 0.4.1 node-htmlprocessor@0.2.4
- 0.4.0 Update Grunt to 1.0
- 0.3.13 node-htmlprocessor@0.2.3 and clone data object (#85)
- 0.3.12 Update [node-htmlprocessor](https://github.com/dciccale/node-htmlprocessor) to version 0.2.2 (escape regex from remove)
- 0.3.11 [get ready for grunt v1.0.0](https://github.com/gruntjs/grunt-docs/blob/master/Blog-2016-02-11-Grunt-1.0.0-rc1-released.md#peer-dependencies)
- 0.3.10 Update [node-htmlprocessor](https://github.com/dciccale/node-htmlprocessor) to version 0.2.1
- 0.3.9 Update [node-htmlprocessor](https://github.com/dciccale/node-htmlprocessor) to version 0.2.0
- 0.3.8 Fix #74
- 0.3.7 Update [node-htmlprocessor](https://github.com/dciccale/node-htmlprocessor) dependency with added `inline` modifier
- 0.3.6 Update node-htmlprocessor version and add specific test for templates
- 0.3.5 Fixes issue when passing data to a `template`
- 0.3.4 Fixes issue when passing a path te replace an `[attr]`
- 0.3.3 Add [node-htmlprocessor](https://github.com/dciccale/node-htmlprocessor) as a dependency
- 0.3.2 Fix/feature #39
- 0.3.1 Fix #35
- 0.3.0 Allow creating custom block types.
- 0.2.9 Added `recursive` option
- 0.2.8 Changed `include` to not use `replace()`
- 0.2.7 Added `commentMarker` option
- 0.2.6 Fix #14 and added grunt-release
- 0.2.5 Create first tag using grunt-release
- 0.2.3 Fix #8
- 0.2.2 Small code refactor
- 0.2.1 Added `templateSettings` option tu customize template delimiters
- 0.2.0 Added the `build:include` feature to include any external file
- 0.1.1 Lint js files inside tasks/lib/
- 0.1.0 Initial release
