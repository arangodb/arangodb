## v1.7.1-f (2014-06-11)

Features:

  - Allow .liticed and iced.md suffices for "literate" iced (via @bwin)
  - Close #123 -- allow debugger to work in a loop

## v1.7.1-e (2014-06-04)

Bugfixes:

  - Fix a problem with registering modules as pointed out by @icflorescu 

## v1.7.1-d (2014-06-04)

Bugfixes:

  - Close #121: allow `iced foo.iced` from anywhere, even if you don't have 
    `iced-runtime` installed globally or locally -- just make a run mode that
    looks for it internalls to the compiler/interpreter.

Tweaks:

  - Try this: `iced = require('iced-runtime')`, as opposed to: 
    `iced = require('iced-runtime').iced;`.  This puts the runtime
    and the library features at the same level. This is more natural
    I think...

## v1.7.1-c (2014-06-03)

Features:

  - Factor out runtime, which is now available via `iced-runtime`
  - Build the browser package via browserify, not via ad-hoc mechanism
  - Build both coffee-script.js and coffee-script-min.js, now renamed
    to iced-coffee-script-#{VERSION}.js and iced-coffee-script-#{VERSION}-min.js
  - Remove other build packages, since now the main library is sucked in with
    browserify.

Warnings:

  - Danger ahead! There's a chance that this release is going to break
    existing software, but it's worth it for the long-haul.  Factoring
    out the runtime means software built with iced has way fewer
    dependencies.
