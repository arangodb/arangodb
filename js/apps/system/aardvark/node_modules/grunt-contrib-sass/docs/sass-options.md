# Options


## sourcemap

Type: `String`  
Default: `auto`

Values:

- `auto` - relative paths where possible, file URIs elsewhere
- `file` - always absolute file URIs
- `inline` - include the source text in the sourcemap
- `none`- no sourcemaps

**Requires Sass 3.4.0, which can be installed with `gem install sass`**


## trace

Type: `Boolean`  
Default: `false`

Show a full traceback on error.


## unixNewlines

Type: `Boolean`  
Default: `false` on Windows, otherwise `true`

Force Unix newlines in written files.


## check

Type: `Boolean`  
Default: `false`

Just check the Sass syntax, does not evaluate and write the output.


## style

Type: `String`  
Default: `nested`

Output style. Can be `nested`, `compact`, `compressed`, `expanded`.


## precision

Type: `Number`  
Default: `5`

How many digits of precision to use when outputting decimal numbers.


## quiet

Type: `Boolean`  
Default: `false`

Silence warnings and status messages during compilation.


## compass

Type: `Boolean`  
Default: `false`

Make Compass imports available and load project configuration (`config.rb` located close to the `Gruntfile.js`).


## debugInfo

Type: `Boolean`  
Default: `false`

Emit extra information in the generated CSS that can be used by the FireSass Firebug plugin.


## lineNumbers

Type: `Boolean`  
Default: `false`

Emit comments in the generated CSS indicating the corresponding source line.


## loadPath

Type: `String|Array`

Add a (or multiple) Sass import path.


## require

Type: `String|Array`

Require a (or multiple) Ruby library before running Sass.


## cacheLocation

Type: `String`  
Default: `.sass-cache`

The path to put cached Sass files.


## noCache

Type: `Boolean`  
Default: `false`

Don't cache to sassc files.


## bundleExec

Type: `Boolean`  
Default: `false`

Run `sass` with [bundle exec](http://gembundler.com/man/bundle-exec.1.html): `bundle exec sass`.


## banner

Type: `String`  

Prepend the specified string to the output file. Useful for licensing information.

*Can't be used if you use the `sourcemap` option.*


## update

Type: `Boolean`  
Default: `false`

Only compile changed files.
