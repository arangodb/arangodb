!CHAPTER Manifest

Many of the fields that were required in ArangoDB 2.x are now optional and can be safely omitted.

To avoid compatibility problems with future versions of ArangoDB you should always specify the `engines` field, e.g.:

```json
{
  "engines": {
    "arangodb": "^3.0.0"
  }
}
```

!SECTION Controllers & exports

Previously Foxx distinguished between `exports` and `controllers`, each of which could be specified as an object. In ArangoDB 3.0 these have been merged into a single `main` field specifying an entry file.

The easiest way to migrate services using multiple exports and/or controllers is to create a separate entry file that imports these files:

Old (manifest.json):

```json
{
  "exports": {
    "doodads": "doodads.js",
    "dingbats": "dingbats.js"
  },
  "controllers": {
    "/doodads": "routes/doodads.js",
    "/dingbats": "routes/dingbats.js",
    "/": "routes/root.js"
  }
}
```

New (manifest.json):

```json
{
  "main": "index.js"
}
```

New (index.js):

```js
'use strict';
module.context.use('/doodads', require('./routes/doodads'));
module.context.use('/dingbats', require('./routes/dingbats'));
module.context.use('/', require('./routes/root'));
module.exports = {
  doodads: require('./doodads'),
  dingbats: require('./dingbats')
};
```

!SECTION Index redirect

If you previously did not define the `defaultDocument` field, please note that in ArangoDB 3.0 the field will no longer default to the value `index.html` when omitted:

Old:

```json
{
  // no defaultDocument
}
```

New:

```json
{
  "defaultDocument": "index.html"
}
```

This also means it is no longer necessary to specify the `defaultDocument` field with an empty value to prevent the redirect and be able to serve requests at the `/` (root) path of the mount point:

Old:

```json
{
  "defaultDocument": ""
}
```

New:

```json
{
  // no defaultDocument
}
```

!SECTION Assets

The `assets` field is no longer supported in ArangoDB 3.0 outside of legacy compatibility mode.

If you previously used the field to serve individual files as-is you can simply use the `files` field instead:

Old:

```json
{
  "assets": {
    "client.js": {
      "files": ["assets/client.js"],
      "contentType": "application/javascript"
    }
  }
}
```

New:

```json
{
  "files": {
    "client.js": {
      "path": "assets/client.js",
      "type": "application/javascript"
    }
  }
}
```

If you relied on being able to specify multiple files that should be concatenated, you will have to use build tools outside of ArangoDB to prepare these files accordingly.

!SECTION Root element

The `rootElement` field is no longer supported and has been removed entirely.

If your controllers relied on this field being available you need to adjust your schemas and routes to be able to handle the full JSON structure of incoming documents.

!SECTION System services

The `isSystem` field is no longer supported. The presence or absence of the field had no effect in most recent versions of ArangoDB 2.x and has now been removed entirely.
