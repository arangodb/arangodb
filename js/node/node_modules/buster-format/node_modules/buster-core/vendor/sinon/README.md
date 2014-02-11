# Sinon.JS

[![Build status](https://secure.travis-ci.org/cjohansen/Sinon.JS.png?branch=master)](http://travis-ci.org/cjohansen/Sinon.JS)

Standalone and test framework agnostic JavaScript test spies, stubs and mocks.

## Installation

via [npm (node package manager)](http://github.com/isaacs/npm)

    $ npm install sinon

or install via git by cloning the repository and including sinon.js
in your project, as you would any other third party library.

Don't forget to include the parts of Sinon.JS that you want to use as well
(i.e. spy.js).

## Usage

See the [sinon project homepage](http://sinonjs.org/)

## Goals

* No global pollution
* Easy to use
* Require minimal “integration”
* Easy to embed seamlessly with any testing framework
* Easily fake any interface
* Ship with ready-to-use fakes for XMLHttpRequest, timers and more

## Contribute?

Pick [an issue](http://github.com/cjohansen/Sinon.JS/issues) to fix, or pitch
new features. To avoid wasting your time, please ask for feedback on feature
suggestions either with [an issue](http://github.com/cjohansen/Sinon.JS/issues/new)
or on [the mailing list](http://groups.google.com/group/sinonjs).

## Run the tests

The Sinon.JS developer environment requires Node/NPM. Please make sure you have
Node installed, and install Sinon's dependencies:

    $ npm install

### On Node

    $ npm test

### In the browser

Open `test/sinon.html` in a browser. To test against a built distribution, first
make sure you have a build (requires Ruby and Juicer):

    $ ./build

Then open `test/sinon-dist.html` in a browser.

Some tests needs working XHR to pass. To run the tests over an HTTP server, run

    $ node_modules/http-server/bin/http-server

Then open [localhost:8080/test/sinon.html](http://localhost:8080/test/sinon.html)
in a browser.

### On Rhino

The Rhino tests are currently out of commission (pending update after switch to
Buster.JS for tests).
