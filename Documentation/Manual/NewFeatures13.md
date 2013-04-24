New Features in ArangoDB 1.3 {#NewFeatures13}
=============================================

@NAVIGATE_NewFeatures13
@EMBEDTOC{NewFeatures13TOC}

Features and Improvements {#NewFeatures13Introduction}
======================================================

The following list shows in detail which features have been added or
improved in ArangoDB 1.3.  ArangoDB 1.3 also contains several bugfixes
that are not listed here.

Changes to the Datafile Structure{#NewFeatures13Datafile}
---------------------------------------------------------

As the datafile structure has changed, please read the 
@ref Upgrading13 "upgrade manual" carefully.

Rapid API Development with FOXX{#NewFeatures13Foxx}
---------------------------------------------------

Foxx is a lightweight Javascript “micro framework” which allows you to
build applications directly on top of ArangoDB and therefore skip the
middleman (Rails, Django, Symfony or whatever your favorite web
framework is). Inspired by frameworks like Sinatra Foxx is designed
with simplicity and the specific use case of modern client-side MVC
frameworks in mind.

The screencast at
<a href="http://foxx.arangodb.org">http://foxx.arangodb.org</a>
explains how to use Foxx.

Transactions{#NewFeatures13Transactions}
----------------------------------------

Node Modules and Packages{#NewFeatures13Node}
---------------------------------------------

ArangoDB 1.3 supports some of @ref JSModulesNode "modules" and @ref JSModulesNPM
"packages" from node. The most important module is maybe the Buffer support,
which allows to handle binary data in JavaScript.

    arangosh> var Buffer = require("buffer").Buffer;
    arangosh> a = new Buffer("414243", "hex");
    ABC
    arangosh> a = new Buffer("414243", "ascii");
    414243
    arangosh> a = new Buffer([48, 49, 50]);
    012

Supplying the Buffer class makes it possible to use other interesting modules
like punycode. It enables us to support some of NPM packages available - for
instance CoffeScript.

    arangosh> var cs = require("coffee-script");
    arangosh> cs.compile("a = 1");
    (function() {
      var a;

      a = 1;

    }).call(this);

    arangosh> cs.compile("square = x -> x * x", { bare: true });
    var square;

    square = x(function() {
      return x * x;
    });

"underscore" is also preinstalled.

    arangosh> var _ = require("underscore");
    arangosh> _.map([1,2,3], function(x) {return x*x;});
    [ 
      1, 
      4, 
      9 
    ]

The node packages can be installed using npm in the "share/npm"
directory. If you find out, that a node package is also working
under ArangoDB, please share your findings with us and other
users.
