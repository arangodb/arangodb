# uri-path
[![NPM version](https://badge.fury.io/js/uri-path.png)](https://npmjs.org/package/uri-path)
[![Build Status](https://travis-ci.org/UltCombo/uri-path.png?branch=master)](https://travis-ci.org/UltCombo/uri-path)
[![devDependency Status](https://david-dm.org/UltCombo/uri-path/dev-status.png)](https://david-dm.org/UltCombo/uri-path#info=devDependencies)

Convert relative file system paths into safe URI paths

# Install

```
npm install --save uri-path
```

# Usage

```js
var URIpath = require('uri-path');

// Properly encode URI path segments
URIpath('../abc/@#$%¨&()[]{}-_=+ß/môòñ 月 قمر');
// -> '../abc/%40%23%24%25%C2%A8%26()%5B%5D%7B%7D-_%3D%2B%C3%9F/m%C3%B4%C3%B2%C3%B1%20%E6%9C%88%20%D9%82%D9%85%D8%B1'

// Also supports Windows backslash paths
URIpath('a\\b\\c');
// -> 'a/b/c'
```
