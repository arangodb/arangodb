/*jshint globalstrict:false, strict:false */
/*global describe, it, expect, beforeEach */
(function () {
'use strict';

const createRouter = require('@arangodb/foxx/router');

const $_WILDCARD = Symbol.for('@@wildcard'); // catch-all suffix
const $_TERMINAL = Symbol.for('@@terminal'); // terminal -- routes be here
const $_PARAM = Symbol.for('@@parameter'); // named parameter (no routes here, like static part)
const $_ROUTES = Symbol.for('@@routes'); // routes and child routers
const $_MIDDLEWARE = Symbol.for('@@middleware'); // middleware (not including router.all)

// const _ = require('lodash');
// function log(something) {
//   let seen = new Set();
//   function objectify(thing) {
//     if (seen.has(thing)) return thing;
//     seen.add(thing);
//     if (!thing) return thing;
//     if (Array.isArray(thing)) return thing.map(objectify);
//     if (typeof thing === 'function') return String(thing);
//     if (typeof thing === 'symbol') return String(thing);
//     if (typeof thing !== 'object') return thing;
//     if (thing.handler) {
//       return `Context(${thing.handler})`;
//     }
//     if (thing._tree) {
//       return {
//         '@@router': objectify(thing._tree)
//       };
//     }
//     if (thing.router) {
//       return {
//         path: thing.path,
//         '@@router': objectify(thing.router._tree)
//       };
//     }
//     let obj = {};
//     if (thing instanceof Map) {
//       for (let entry of thing.entries()) {
//         obj[String(entry[0])] = objectify(entry[1]);
//       }
//     } else {
//       _.each(thing, function (value, key) {
//         obj[String(key)] = objectify(value);
//       });
//     }
//     return obj;
//   }
//   console.infoLines(JSON.stringify(objectify(something), null, 2));
// }

describe('Router', function () {
  let router, childRouter1, childRouter2;
  let GET_SLASH;
  let USE_SLASH;
  let POST_HELLO;
  let GET_HELLO_WORLD;
  let USE_HELLO_WORLD;
  let GET_HELLO_PARAM;
  let USE_HELLO_PARAM;
  let GET_WORLD;
  let GET_SLASH2;
  let GET_ALL;
  let GET_HELLO_WORLD2;
  let GET_POTATO_SALAD1;
  let GET_POTATO_SALAD2;

  function prepareRouter(router, childRouter1, childRouter2) {
    router.use('/hello', childRouter1);
    router.use(childRouter2);
    GET_SLASH = router.get('/', function() {/*GET /*/});
    USE_SLASH = router.use(function() {/*middleware /*/});
    POST_HELLO = router.post('/hello', function() {/*POST /hello*/});
    GET_HELLO_WORLD = router.get('/hello/world', function() {/*GET /hello/world*/});
    USE_HELLO_WORLD = router.use('/hello/world', function () {/*middleware /hello/world*/});
    GET_HELLO_PARAM = router.get('/hello/:symbol', function() {/*GET /hello/:symbol*/});
    USE_HELLO_PARAM = router.use('/hello/:thang', function () {/*middleware /hello/:thang*/});
    GET_WORLD = childRouter1.get('/world', function() {/*1>GET /world*/});
    GET_SLASH2 = childRouter2.get(function() {/*2>GET /*/});
    GET_ALL = childRouter2.get('/*', function() {/*2>GET /...*/});
    GET_HELLO_WORLD2 = childRouter2.get('/hello/world', function() {/*2>GET /hello/world*/});
    GET_POTATO_SALAD1 = childRouter2.get('/potato/salad', function() {/*2>GET /potato/salad 1*/});
    GET_POTATO_SALAD2 = childRouter2.get('/potato/salad', function() {/*2>GET /potato/salad 2*/});
  }
  beforeEach(function () {
    router = createRouter();
    childRouter1 = createRouter();
    childRouter2 = createRouter();
    prepareRouter(router, childRouter1, childRouter2);
  });
  describe('_buildRouteTree', function () {
    it('creates the correct tree', function () {
      router._buildRouteTree();
      const tree = router._tree;
      expect(tree.size).toBe(3);
      expect(tree.get($_TERMINAL).size).toBe(1);
      expect(tree.get($_TERMINAL).get($_ROUTES)).toEqual([GET_SLASH]);
      expect(tree.get('hello').size).toBe(4);
      expect(tree.get('hello').get($_TERMINAL).size).toBe(1);
      expect(tree.get('hello').get($_TERMINAL).get($_ROUTES)).toEqual([POST_HELLO]);
      expect(tree.get('hello').get('world').size).toBe(2);
      expect(tree.get('hello').get('world').get($_TERMINAL).size).toBe(1);
      expect(tree.get('hello').get('world').get($_TERMINAL).get($_ROUTES)).toEqual([GET_HELLO_WORLD]);
      expect(tree.get('hello').get('world').get($_WILDCARD).size).toBe(1);
      expect(tree.get('hello').get('world').get($_WILDCARD).get($_MIDDLEWARE)).toEqual([USE_HELLO_WORLD]);
      expect(tree.get('hello').get($_PARAM).size).toBe(2);
      expect(tree.get('hello').get($_PARAM).get($_TERMINAL).size).toBe(1);
      expect(tree.get('hello').get($_PARAM).get($_TERMINAL).get($_ROUTES)).toEqual([GET_HELLO_PARAM]);
      expect(tree.get('hello').get($_PARAM).get($_WILDCARD).size).toBe(1);
      expect(tree.get('hello').get($_PARAM).get($_WILDCARD).get($_MIDDLEWARE)).toEqual([USE_HELLO_PARAM]);
      expect(tree.get('hello').get($_WILDCARD).size).toBe(1);
      expect(tree.get('hello').get($_WILDCARD).get($_ROUTES).length).toBe(1);
      const child1 = tree.get('hello').get($_WILDCARD).get($_ROUTES)[0].router._tree;
      expect(child1.size).toBe(1);
      expect(child1.get('world').size).toBe(1);
      expect(child1.get('world').get($_TERMINAL).size).toBe(1);
      expect(child1.get('world').get($_TERMINAL).get($_ROUTES)).toEqual([GET_WORLD]);
      expect(tree.get($_WILDCARD).size).toBe(2);
      expect(tree.get($_WILDCARD).get($_MIDDLEWARE)).toEqual([USE_SLASH]);
      const child2 = tree.get($_WILDCARD).get($_ROUTES)[0].router._tree;
      expect(child2.size).toBe(4);
      expect(child2.get($_TERMINAL).size).toBe(1);
      expect(child2.get($_TERMINAL).get($_ROUTES)).toEqual([GET_SLASH2]);
      expect(child2.get('hello').size).toBe(1);
      expect(child2.get('hello').get('world').size).toBe(1);
      expect(child2.get('hello').get('world').get($_TERMINAL).size).toBe(1);
      expect(child2.get('hello').get('world').get($_TERMINAL).get($_ROUTES)).toEqual([GET_HELLO_WORLD2]);
      expect(child2.get('potato').size).toBe(1);
      expect(child2.get('potato').get('salad').size).toBe(1);
      expect(child2.get('potato').get('salad').get($_TERMINAL).size).toBe(1);
      expect(child2.get('potato').get('salad').get($_TERMINAL).get($_ROUTES)).toEqual([GET_POTATO_SALAD1, GET_POTATO_SALAD2]);
      expect(child2.get($_WILDCARD).size).toBe(1);
      expect(child2.get($_WILDCARD).get($_ROUTES)).toEqual([GET_ALL]);
    });
  });
  describe('_findRoutes', function () {
    beforeEach(function () {
      router._buildRouteTree();
    });
    it('finds all routes for /', function () {
      const matches = [];
      for (const match of router._findRoutes([])) {
        matches.push(match);
      }
      expect(matches.length).toBe(3);
      expect(matches.some(function (match) {
        if (match.route !== GET_SLASH) {
          return false;
        }
        expect(match.routers).toEqual([router]);
        expect(match.middleware).toEqual([[USE_SLASH]]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_SLASH2) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter2]);
        expect(match.middleware).toEqual([[USE_SLASH]]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_ALL) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter2]);
        expect(match.middleware).toEqual([[USE_SLASH]]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
    });
    it('finds all routes for /hello', function () {
      const matches = [];
      for (const match of router._findRoutes(['hello'])) {
        matches.push(match);
      }
      expect(matches.length).toBe(2);
      expect(matches.some(function (match) {
        if (match.route !== POST_HELLO) {
          return false;
        }
        expect(match.routers).toEqual([router]);
        expect(match.middleware).toEqual([[USE_SLASH], []]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_ALL) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter2]);
        expect(match.middleware).toEqual([[USE_SLASH]]);
        expect(match.suffix).toEqual(['hello']);
        return true;
      })).toBe(true);
    });
    it('finds all routes for /hello/world', function () {
      const matches = [];
      for (const match of router._findRoutes(['hello', 'world'])) {
        matches.push(match);
      }
      expect(matches.length).toBe(5);
      expect(matches.some(function (match) {
        if (match.route !== GET_HELLO_WORLD) {
          return false;
        }
        expect(match.routers).toEqual([router]);
        expect(match.middleware).toEqual([[USE_SLASH], [], [USE_HELLO_WORLD]]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_HELLO_PARAM) {
          return false;
        }
        expect(match.routers).toEqual([router]);
        expect(match.middleware).toEqual([[USE_SLASH], [], [USE_HELLO_PARAM]]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_WORLD) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter1]);
        expect(match.middleware).toEqual([[USE_SLASH], [], []]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_HELLO_WORLD2) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter2]);
        expect(match.middleware).toEqual([[USE_SLASH], [], []]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_ALL) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter2]);
        expect(match.middleware).toEqual([[USE_SLASH]]);
        expect(match.suffix).toEqual(['hello', 'world']);
        return true;
      })).toBe(true);
    });
    it('finds all routes for /hello/mlady', function () {
      const matches = [];
      for (const match of router._findRoutes(['hello', 'mlady'])) {
        matches.push(match);
      }
      expect(matches.length).toBe(2);
      expect(matches.some(function (match) {
        if (match.route !== GET_HELLO_PARAM) {
          return false;
        }
        expect(match.routers).toEqual([router]);
        expect(match.middleware).toEqual([[USE_SLASH], [], [USE_HELLO_PARAM]]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_ALL) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter2]);
        expect(match.middleware).toEqual([[USE_SLASH]]);
        expect(match.suffix).toEqual(['hello', 'mlady']);
        return true;
      })).toBe(true);
    });
    it('finds all routes for /potato/salad', function () {
      const matches = [];
      for (const match of router._findRoutes(['potato', 'salad'])) {
        matches.push(match);
      }
      expect(matches.length).toBe(3);
      expect(matches.some(function (match) {
        if (match.route !== GET_POTATO_SALAD1) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter2]);
        expect(match.middleware).toEqual([[USE_SLASH], [], []]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_POTATO_SALAD2) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter2]);
        expect(match.middleware).toEqual([[USE_SLASH], [], []]);
        expect(match.suffix).toEqual([]);
        return true;
      })).toBe(true);
      expect(matches.some(function (match) {
        if (match.route !== GET_ALL) {
          return false;
        }
        expect(match.routers).toEqual([router, childRouter2]);
        expect(match.middleware).toEqual([[USE_SLASH]]);
        expect(match.suffix).toEqual(['potato', 'salad']);
        return true;
      })).toBe(true);
    });
  });
});

}());
