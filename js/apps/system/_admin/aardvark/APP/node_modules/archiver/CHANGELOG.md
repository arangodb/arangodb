## Changelog

**1.3.0** — <small> December 13, 2016_</small> — [Diff](https://github.com/archiverjs/node-archiver/compare/1.2.0...1.3.0)

- improve `directory` and `glob` methods to use events rather than callbacks. (#203)
- fix bulk warning spam (#208)
- updated mocha (#205)

**1.2.0** — <small> November 2, 2016_</small> — [Diff](https://github.com/archiverjs/node-archiver/compare/1.1.0...1.2.0)

- Add a `process.emitWarning` for `deprecated` (#202)

**1.1.0** — <small> August 29, 2016_</small> — [Diff](https://github.com/archiverjs/node-archiver/compare/1.0.1...1.1.0)

- minor doc fixes.
- bump deps to ensure latest versions are used.

**1.0.1** — <small>_July 27, 2016_</small> — [Diff](https://github.com/archiverjs/node-archiver/compare/1.0.0...1.0.1)

- minor doc fixes.
- dependencies upgraded.

**1.0.0** — <small>_April 5, 2016_</small> — [Diff](https://github.com/archiverjs/node-archiver/compare/0.21.0...1.0.0)

- version unification across many archiver packages.
- dependencies upgraded and now using semver caret (^).

**0.21.0** — <small>_December 21, 2015_</small> — [Diff](https://github.com/archiverjs/node-archiver/compare/0.20.0...0.21.0)

- core: add support for `entry.prefix`. update some internals to use it.
- core(glob): when setting `options.cwd` get an absolute path to the file and use the relative path for `entry.name`. #173
- core(bulk): soft-deprecation of `bulk` feature. will remain for time being with no new features or support.
- docs: initial jsdoc for core. http://archiverjs.com/docs
- tests: restructure a bit.

**0.20.0** — <small>_November 30, 2015_</small> — [Diff](https://github.com/archiverjs/node-archiver/compare/0.19.0...0.20.0)

- simpler path normalization as path.join was a bit restrictive. #162
- move utils to separate module to DRY.

[Release Archive](https://github.com/archiverjs/node-archiver/releases)