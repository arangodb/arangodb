# prebuild-install

[![Build Status](https://travis-ci.org/prebuild/prebuild-install.svg?branch=master)](https://travis-ci.org/prebuild/prebuild-install)
[![js-standard-style](https://img.shields.io/badge/code%20style-standard-brightgreen.svg)](http://standardjs.com/)

> A command line tool for easily install prebuilds for multiple version of node/iojs on a specific platform.

`prebuild-install` supports installing prebuilt binaries from GitHub by default.

## Usage

Change your package.json install script to:
```
...
  "scripts": {
    "install": "prebuild-install || node-gyp rebuild"
  }
...
```

### Requirements

You need to provide prebuilds made by [prebuild](https://github.com/mafintosh/prebuild)

### Help
```
prebuild-install [options]

  --download    -d  [url]       (download prebuilds, no url means github)
  --target      -t  version     (version to install for)
  --runtime     -r  runtime     (Node runtime [node or electron] to build or install for, default is node)
  --path        -p  path        (make a prebuild-install here)
  --build-from-source           (skip prebuild download)
  --verbose                     (log verbosely)
  --libc                        (use provided libc rather than system default)
  --debug                       (set Debug or Release configuration)
  --version                     (print prebuild-install version and exit)
 ```

When `prebuild-install` is run via an `npm` script, options
`--build-from-source`, `--debug` and `--download`, may be passed through via
arguments given to the `npm` command.

### Custom binaries
The end user can override binary download location through environment variables in their .npmrc file.  
The variable needs to meet the mask `% your package name %_binary_host` or `% your package name %_binary_host_mirror`. For example:
```
leveldown_binary_host=http://overriden-host.com/overriden-path
```
Note that the package version subpath and file name will still be appended.  
So if you are installing `leveldown@1.2.3` the resulting url will be:  
```
http://overriden-host.com/overriden-path/v1.2.3/leveldown-v1.2.3-node-v57-win32-x64.tar.gz
```

## License

MIT
