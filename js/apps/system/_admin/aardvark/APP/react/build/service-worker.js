/**
 * Welcome to your Workbox-powered service worker!
 *
 * You'll need to register this file in your web app and you should
 * disable HTTP caching for this file too.
 * See https://goo.gl/nhQhGp
 *
 * The rest of the code is auto-generated. Please don't update this file
 * directly; instead, make changes to your Workbox build configuration
 * and re-run your build process.
 * See https://goo.gl/2aRDsh
 */

importScripts("https://storage.googleapis.com/workbox-cdn/releases/3.6.3/workbox-sw.js");

importScripts(
<<<<<<< HEAD
<<<<<<< HEAD
  "precache-manifest.59c82c9b75a3e465f1f55f8afabb6232.js"
=======
  "precache-manifest.67ab79788985b19f337605fcf3085c90.js"
>>>>>>> 549c8a77b8a0ee3562c2566374c7cd50e2b8c6ee
=======
  "precache-manifest.498fbaf455067ad4d2cf279f7347c4ff.js"
>>>>>>> 29dcd14374017e431f5641c7a0d33553b1015de9
);

workbox.clientsClaim();

/**
 * The workboxSW.precacheAndRoute() method efficiently caches and responds to
 * requests for URLs in the manifest.
 * See https://goo.gl/S9QRab
 */
self.__precacheManifest = [].concat(self.__precacheManifest || []);
workbox.precaching.suppressWarnings();
workbox.precaching.precacheAndRoute(self.__precacheManifest, {});

workbox.routing.registerNavigationRoute("/index.html", {
  
  blacklist: [/^\/_/,/\/[^/]+\.[^/]+$/],
});
