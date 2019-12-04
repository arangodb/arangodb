#OPTIONAL

Node-optional allows you to optionally 'require' modules without surrounding everything with 'try/catch'.  Usage and installation is easy and this module itself is very easy and straightforward to use.

##Install

```
  npm install optional
```

##Usage

```javascript
var optional = require("./optional");

var express = optional("express");
var fs = optional("fs");

console.log("express: " + express);
console.log("fs: " + fs);
```

Output:
```
express: null
fs: [object Object]
```

##Changelog
###v0.1.0-2

 * Corrected bug when trying to optionally include relative paths
