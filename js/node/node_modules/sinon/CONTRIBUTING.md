# Contributing to Sinon.JS

There are several ways of contributing to Sinon.JS

* Help [improve the documentation](https://github.com/sinonjs/sinon-docs) published at [the Sinon.JS website](http://sinonjs.org)
* Help someone understand and use Sinon.JS on the [the mailing list](http://groups.google.com/group/sinonjs)
* Report an issue, please read instructions below
* Help with triaging the [issues](http://github.com/cjohansen/Sinon.JS/issues). The clearer they are, the more likely they are to be fixed soon.
* Contribute to the code base.

## Reporting an issue

To save everyone time and make it much more likely for your issue to be understood, worked on and resolved quickly, it would help if you're mindful of [How to Report Bugs Effectively](http://www.chiark.greenend.org.uk/~sgtatham/bugs.html) when pressing the "Submit new issue" button.

As a minimum, please report the following:

* Which environment are you using? Browser? Node? Which version(s)?
* Which version of SinonJS?
* How are you loading SinonJS?
* What other libraries are you using?
* What you expected to happen
* What actually happens
* Describe **with code** how to reproduce the faulty behaviour

### Bug report template

Here's a template for a bug report

> Sinon version :
> Environment   :
> Example URL   :
>
> ##### Bug description

Here's an example use of the template

> Sinon version : 1.10.3
> Environment   : OSX Chrome 37.0.2062.94
> Example URL   : http://jsbin.com/iyebiWI/8/edit
>
> ##### Bug description
>
> If `respondWith` called with a URL including query parameter and a function , it doesn't work.
> This error reported in console.
> ```
> `TypeError: requestUrl.match(...) is null`
> ```

## Contributing to the code base

Pick [an issue](http://github.com/cjohansen/Sinon.JS/issues) to fix, or pitch
new features. To avoid wasting your time, please ask for feedback on feature
suggestions either with [an issue](http://github.com/cjohansen/Sinon.JS/issues/new)
or on [the mailing list](http://groups.google.com/group/sinonjs).

### Use EditorConfig

To save everyone some time, please use [EditorConfig](http://editorconfig.org), so your editor helps make
sure we all use the same encoding, indentation, line endings, etc.

### Installation

The Sinon.JS developer environment requires Node/NPM. Please make sure you have
Node installed, and install Sinon's dependencies:

    $ npm install

### Style

Sinon.JS uses [JSCS](https://github.com/mdevils/node-jscs) to keep consistent style. You probably want to install one of their plugins for your editor.

The JSCS test will be run before unit tests in the CI environment, your build will fail if it doesn't pass the style check.

You can run the jscs test with

```
$ npm run lint
```

or if you have JSCS installed as a global

```
$ jscs .
```

### Run the tests

#### On Node

    $ npm test

#### In the browser

Some tests needs working XHR to pass. To run the tests over an HTTP server, run

    $ node_modules/http-server/bin/http-server

##### Testing in development

Open [localhost:8080/test/sinon.html](http://localhost:8080/test/sinon.html) in a browser.

##### Testing a built version

To test against a built distribution, first
make sure you have a build (requires [Ruby][ruby] and [Juicer][juicer]):

    $ ./build

[ruby]: https://www.ruby-lang.org/en/
[juicer]: http://rubygems.org/gems/juicer

If the build script is unable to find Juicer, try

    $ ruby -rubygems build

Open [localhost:8080/test/sinon-dist.html](http://localhost:8080/test/sinon-dist.html) in a browser.

#### In PhantomJS

If you have [PhantomJS](http://phantomjs.org) installed as a global, you can run the test suite in PhantomJS

```
$ test/phantom/run.sh
```
