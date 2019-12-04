# htmlprocessor

[![NPM version](https://img.shields.io/npm/v/htmlprocessor.svg)](https://www.npmjs.com/package/htmlprocessor)
[![Build status](https://travis-ci.org/dciccale/node-htmlprocessor.svg?branch=master)](https://travis-ci.org/dciccale/node-htmlprocessor)
[![NPM downloads](https://img.shields.io/npm/dm/htmlprocessor.svg)](https://www.npmjs.com/package/htmlprocessor)
[![Cove coverage](https://img.shields.io/coveralls/dciccale/node-htmlprocessor.svg)](https://coveralls.io/github/dciccale/node-htmlprocessor)


`npm install -g htmlprocessor`

### Grunt/Gulp task

This module is the processor behind [grunt-processhtml](https://www.npmjs.com/package/grunt-processhtml), [gulp-processhtml](https://www.npmjs.com/package/gulp-processhtml) tasks.

For plenty of examples visit the [documentation](https://github.com/dciccale/grunt-processhtml#readme).

## CLI

Outputs help
```sh
$ htmlprocessor -h
Usage: htmlprocessor file-to-process.html [options]

  -h, --help             display this help message
  -v, --version          display the version number
  -l, --list             file to output list of replaced files
  -o, --output           file to output processed HTML to
  -d, --data             pass a JSON file to processor
  -e, --env              specify an environment
  -r, --recursive        recursive processing
  -c, --comment-marker   change the comment marker
  -i, --include-base     set the directory to include files from
  -s, --strip            strip blocks matched by other environments
  --custom-block-type    specify custom block type
```

Outputs version number
```sh
$ htmlprocessor -v
```

Outputs to `file-to-process.processed.html`.

```sh
$ htmlprocessor file-to-process.html
```

Outputs to `processed/file.html`.

```sh
$ htmlprocessor file-to-process.html -o processed/file.html
```

Pass some data

```sh
$ htmlprocessor file-to-process.html -o processed/file.html -d data.json
```

Specify an environment

```sh
$ htmlprocessor file-to-process.html -o processed/file.html -e dev
```

Allow recursive processing

```sh
$ htmlprocessor file-to-process.html -o processed/file.html -r
```

Change the comment marker to `<!-- process --><!-- /process -->`

```sh
$ htmlprocessor file-to-process.html -o processed/file.html --comment-marker process
```

## List option

Create a list of files that were replaced and use that list to streamline the build process.

Note: This new option does not affect in any way the previous existing functionality (i.e. it's backward compatible).

```sh
$ htmlprocessor file-to-process.html -o processed/file.html --list wrk/replacement.list
```

Assumning you have this code in an HTML (or JSP)

```sh
  .
  .
  .
  <!-- build:css content/myApplication.min.css -->
  <link rel="stylesheet" href="js/bower_components/bootstrap/dist/css/bootstrap.css" />
  <link rel="stylesheet" href="content/bootstrap-responsive.min.css" needed />
  <link rel="stylesheet" href="content/styles.css" />
  <link rel="stylesheet" href="content/myApplicationStyles.css" />
  <!--/build-->
  .
  .
  .
  <!-- build:js js/myApplication.min.js -->
  <script src="js/bower_components/jquery/dist/jquery.js"></script>
  <script src="js/bower_components/angular/angular.js"></script>
  <script src="js/bower_components/angular-route/angular-route.js"></script>

  <!-- App libs -->
  <script src="app/app.js"></script>
  <script src="app/filters/filters.js"></script>
  <script src="app/controllers/applications.js"></script>
  <!--/build-->
  .
  .
  .
```

The file "wrk/replacement.list" will contain something like this:

```sh
file-to-process.html:js/bower_components/bootstrap/dist/css/bootstrap.css
file-to-process.html:content/bootstrap-responsive.min.css
file-to-process.html:content/styles.css
file-to-process.html:content/myApplicationStyles.css
file-to-process.html:js/bower_components/jquery/dist/jquery.js
file-to-process.html:js/bower_components/angular/angular.js
file-to-process.html:js/bower_components/angular-route/angular-route.js
file-to-process.html:app/app.js
file-to-process.html:app/filters/filters.js
file-to-process.html:app/controllers/applications.js
```

And you can use these commands to concatenate and eventually minify without having to update the build to tell
it where it should pickup each files. Also, in this way it orders the global file content in the same manner
as your individual includes originally were.

```sh
sh -c "cat `cat wrk/replacement.list | grep '\.js$' | cut -d: -f2` > dist/js/myApplication.js"
sh -c "cat `cat wrk/replacement.list | grep '\.css$' | cut -d: -f2` > dist/css/myApplication.css"
```

If you processed more than a single "html" file, you can change the grep like this:

```sh
... | grep 'file-to-process.html:.*\.js$' | ... > dist/js/myApplication.js
... | grep 'other-file-to-process.html:.*\.js$' | ... > dist/js/myApplicationOther.js
```

The originating file name is included in the list file for that very purpose.

## License
See [LICENSE.txt](https://raw.github.com/dciccale/node-htmlprocessor/master/LICENSE-MIT)
