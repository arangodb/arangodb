---
layout: default
title: CommonJS API
---

CommonJS
========

JavaScript is a powerful object oriented language with some of the fastest dynamic language interpreters around. The official JavaScript specification defines APIs for some objects that are useful for building browser-based applications. However, the spec does not define a standard library that is useful for building a broader range of applications.

The CommonJS API will fill that gap by defining APIs that handle many common application needs, ultimately providing a standard library as rich as those of Python, Ruby and Java. The intention is that an application developer will be able to write an application using the CommonJS APIs and then run that application across different JavaScript interpreters and host environments. With CommonJS-compliant systems, you can use JavaScript to write:
    
* Server-side JavaScript applications
* Command line tools
* Desktop GUI-based applications
* Hybrid applications (Titanium, Adobe AIR)

Read an [additional introduction by Kris Kowal at Ars Technica](http://arstechnica.com/web/news/2009/12/commonjs-effort-sets-javascript-on-path-for-world-domination.ars).

Current version: [0.1](specs/0.1.html)
In development: [0.5](specs/0.5.html)
    
Implementations
---------------

<table id="implementations" class="tablesorter">
  <thead>
  <tr><th>Project</th><th>API Version</th><th>Interpreter</tr>
  </thead>
  <tbody>
  <tr><td><a href="impl/narwhal.html">Narwhal</a></td><td>0.1+</td><td><a href="interp/rhino.html">Rhino</a>, <a href="interp/mozilla.html">Mozilla</a></td></tr>
  <tr><td><a href="impl/v8cgi.html">v8cgi</a></td><td>0.1+</td><td><a href="interp/v8.html">v8</a></td></tr>
  <tr><td><a href="impl/helma.html">Helma</a></td><td>0.1+</td><td><a href="interp/rhino.html">Rhino</a></td></tr>
  <tr><td><a href="impl/persevere.html">Persevere</a></td><td>0.1+</td><td><a href="interp/rhino.html">Rhino</a></td></tr>
  <tr><td><a href="impl/gpsee.html">GPSEE</a></td><td>0.1+</td><td><a href="interp/spidermonkey.html">SpiderMonkey</a></td></tr>
  <tr><td><a href="impl/flusspferd.html">Flusspferd</a></td><td>0.1+</td><td><a href="interp/spidermonkey.html">SpiderMonkey</a></td></tr>
  <tr><td><a href="impl/monkeyscript.html">MonkeyScript</a></td><td>0.0</td><td><a href="interp/rhino.html">Rhino</a></td></tr>
  </tbody>
</table>
    
More information about CommonJS
-------------------------------

* [Group Process](process.html)
* [Project History](history.html)
