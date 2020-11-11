/* global describe, it, beforeEach */
'use strict';
const Router = require('@arangodb/foxx/router/router');
const Tree = require('@arangodb/foxx/router/tree');
const expect = require('chai').expect;

const $_WILDCARD = Symbol.for('@@wildcard'); // catch-all suffix
const $_TERMINAL = Symbol.for('@@terminal'); // terminal -- routes be here
const $_PARAM = Symbol.for('@@parameter'); // named parameter (no routes here, like static part)
const $_ROUTES = Symbol.for('@@routes'); // routes and child routers
const $_MIDDLEWARE = Symbol.for('@@middleware'); // middleware (not including router.all)

require("@arangodb/test-helper").waitForFoxxInitialized();

// const _ = require('lodash')
// function log(something) {
//   const seen = new Set()
//   function objectify(thing) {
//     if (seen.has(thing)) return thing
//     seen.add(thing)
//     if (!thing) return thing
//     if (Array.isArray(thing)) return thing.map(objectify)
//     if (typeof thing === 'function') return String(thing)
//     if (typeof thing === 'symbol') return String(thing)
//     if (typeof thing !== 'object') return thing
//     if (thing.handler) {
//       return `Context(${thing.handler})`
//     }
//     if (thing.tree) {
//       return {
//         '@@router': objectify(thing.tree.root)
//       }
//     }
//     if (thing.router) {
//       return {
//         path: thing.path,
//         '@@router': objectify(thing.tree.root)
//       }
//     }
//     const obj = {}
//     if (thing instanceof Map) {
//       for (const entry of thing.entries()) {
//         obj[String(entry[0])] = objectify(entry[1])
//       }
//     } else {
//       _.each(thing, function (value, key) {
//         obj[String(key)] = objectify(value)
//       })
//     }
//     return obj
//   }
//   console.infoLines(JSON.stringify(objectify(something), null, 2))
// }

describe('Tree', function () {
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
  let CHILD1;
  let CHILD2;

  function prepareRouter (router, childRouter1, childRouter2) {
    router.use('/hello', childRouter1);
    CHILD1 = router._routes[router._routes.length - 1];
    router.use(childRouter2);
    CHILD2 = router._routes[router._routes.length - 1];
    GET_SLASH = router.get('/', function () {});
    USE_SLASH = router.use(function () {});
    POST_HELLO = router.post('/hello', function () {});
    GET_HELLO_WORLD = router.get('/hello/world', function () {});
    USE_HELLO_WORLD = router.use('/hello/world', function () {});
    GET_HELLO_PARAM = router.get('/hello/:thing', function () {});
    USE_HELLO_PARAM = router.use('/hello/:thang', function () {});
    GET_WORLD = childRouter1.get('/world', function () {});
    GET_SLASH2 = childRouter2.get(function () {});
    GET_ALL = childRouter2.get('/*', function () {});
    GET_HELLO_WORLD2 = childRouter2.get('/hello/world', function () {});
    GET_POTATO_SALAD1 = childRouter2.get('/potato/salad', function () {});
    GET_POTATO_SALAD2 = childRouter2.get('/potato/salad', function () {});
  }

  beforeEach(function () {
    router = new Router();
    childRouter1 = new Router();
    childRouter2 = new Router();
    prepareRouter(router, childRouter1, childRouter2);
  });

  describe('constructor', function () {
    it('creates the correct tree', function () {
      const tree = new Tree({}, router).root;
      expect(tree.size).to.equal(3);
      expect(tree.get($_TERMINAL).size).to.equal(1);
      expect(tree.get($_TERMINAL).get($_ROUTES))
        .to.eql([GET_SLASH]);

      expect(tree.get('hello').size).to.equal(4);
      expect(tree.get('hello').get($_TERMINAL).size).to.equal(1);
      expect(tree.get('hello').get($_TERMINAL).get($_ROUTES))
        .to.eql([POST_HELLO]);

      expect(tree.get('hello').get('world').size).to.equal(2);
      expect(tree.get('hello').get('world').get($_TERMINAL).size).to.equal(1);
      expect(tree.get('hello').get('world').get($_TERMINAL).get($_ROUTES))
        .to.eql([GET_HELLO_WORLD]);

      expect(tree.get('hello').get('world').get($_WILDCARD).size).to.equal(1);
      expect(tree.get('hello').get('world').get($_WILDCARD).get($_MIDDLEWARE))
        .to.eql([USE_HELLO_WORLD]);

      expect(tree.get('hello').get($_PARAM).size).to.equal(2);
      expect(tree.get('hello').get($_PARAM).get($_TERMINAL).size).to.equal(1);
      expect(tree.get('hello').get($_PARAM).get($_TERMINAL).get($_ROUTES))
        .to.eql([GET_HELLO_PARAM]);

      expect(tree.get('hello').get($_PARAM).get($_WILDCARD).size).to.equal(1);
      expect(tree.get('hello').get($_PARAM).get($_WILDCARD).get($_MIDDLEWARE))
        .to.eql([USE_HELLO_PARAM]);

      expect(tree.get('hello').get($_WILDCARD).size).to.equal(1);
      expect(tree.get('hello').get($_WILDCARD).get($_ROUTES).length).to.equal(1);

      const child1 = tree.get('hello').get($_WILDCARD).get($_ROUTES)[0].tree.root;
      expect(child1.size).to.equal(1);
      expect(child1.get('world').size).to.equal(1);
      expect(child1.get('world').get($_TERMINAL).size).to.equal(1);
      expect(child1.get('world').get($_TERMINAL).get($_ROUTES))
        .to.eql([GET_WORLD]);

      expect(tree.get($_WILDCARD).size).to.equal(2);
      expect(tree.get($_WILDCARD).get($_MIDDLEWARE))
        .to.eql([USE_SLASH]);

      const child2 = tree.get($_WILDCARD).get($_ROUTES)[0].tree.root;
      expect(child2.size).to.equal(4);
      expect(child2.get($_TERMINAL).size).to.equal(1);
      expect(child2.get($_TERMINAL).get($_ROUTES))
        .to.eql([GET_SLASH2]);

      expect(child2.get('hello').size).to.equal(1);
      expect(child2.get('hello').get('world').size).to.equal(1);
      expect(child2.get('hello').get('world').get($_TERMINAL).size).to.equal(1);
      expect(child2.get('hello').get('world').get($_TERMINAL).get($_ROUTES))
        .to.eql([GET_HELLO_WORLD2]);

      expect(child2.get('potato').size).to.equal(1);
      expect(child2.get('potato').get('salad').size).to.equal(1);
      expect(child2.get('potato').get('salad').get($_TERMINAL).size).to.equal(1);
      expect(child2.get('potato').get('salad').get($_TERMINAL).get($_ROUTES))
        .to.eql([GET_POTATO_SALAD1, GET_POTATO_SALAD2]);

      expect(child2.get($_WILDCARD).size).to.equal(1);
      expect(child2.get($_WILDCARD).get($_ROUTES))
        .to.eql([GET_ALL]);
    });
  });

  describe('findRoutes', function () {
    let tree;

    beforeEach(function () {
      tree = new Tree({}, router);
    });

    it('finds all routes for /', function () {
      const matches = [];
      for (const route of tree.findRoutes([])) {
        matches.push(route);
      }
      expect(matches.length).to.equal(3);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_SLASH) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: []},
          {middleware: USE_SLASH, path: [], suffix: []},
          {endpoint: GET_SLASH, path: [], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_SLASH2) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: []},
          {middleware: USE_SLASH, path: [], suffix: []},
          {router: CHILD2, path: [], suffix: []},
          {endpoint: GET_SLASH2, path: [], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_ALL) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: []},
          {middleware: USE_SLASH, path: [], suffix: []},
          {router: CHILD2, path: [], suffix: []},
          {endpoint: GET_ALL, path: [], suffix: []}
        ]);
        return true;
      })).to.equal(true);
    });

    it('finds all routes for /hello', function () {
      const matches = [];
      for (const route of tree.findRoutes(['hello'])) {
        matches.push(route);
      }
      expect(matches.length).to.equal(2);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== POST_HELLO) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['hello']},
          {middleware: USE_SLASH, path: [], suffix: ['hello']},
          {endpoint: POST_HELLO, path: ['hello'], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_ALL) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['hello']},
          {middleware: USE_SLASH, path: [], suffix: ['hello']},
          {router: CHILD2, path: [], suffix: ['hello']},
          {endpoint: GET_ALL, path: [], suffix: ['hello']}
        ]);
        return true;
      })).to.equal(true);
    });

    it('finds all routes for /hello/world', function () {
      const matches = [];
      for (const route of tree.findRoutes(['hello', 'world'])) {
        matches.push(route);
      }
      expect(matches.length).to.equal(5);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_HELLO_WORLD) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['hello', 'world']},
          {middleware: USE_SLASH, path: [], suffix: ['hello', 'world']},
          {middleware: USE_HELLO_WORLD, path: ['hello', 'world'], suffix: []},
          {endpoint: GET_HELLO_WORLD, path: ['hello', 'world'], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_HELLO_PARAM) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['hello', 'world']},
          {middleware: USE_SLASH, path: [], suffix: ['hello', 'world']},
          {middleware: USE_HELLO_PARAM, path: ['hello', 'world'], suffix: []},
          {endpoint: GET_HELLO_PARAM, path: ['hello', 'world'], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_WORLD) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['hello', 'world']},
          {middleware: USE_SLASH, path: [], suffix: ['hello', 'world']},
          {router: CHILD1, path: ['hello'], suffix: ['world']},
          {endpoint: GET_WORLD, path: ['world'], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_HELLO_WORLD2) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['hello', 'world']},
          {middleware: USE_SLASH, path: [], suffix: ['hello', 'world']},
          {router: CHILD2, path: [], suffix: ['hello', 'world']},
          {endpoint: GET_HELLO_WORLD2, path: ['hello', 'world'], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_ALL) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['hello', 'world']},
          {middleware: USE_SLASH, path: [], suffix: ['hello', 'world']},
          {router: CHILD2, path: [], suffix: ['hello', 'world']},
          {endpoint: GET_ALL, path: [], suffix: ['hello', 'world']}
        ]);
        return true;
      })).to.equal(true);
    });

    it('finds all routes for /hello/mlady', function () {
      const matches = [];
      for (const route of tree.findRoutes(['hello', 'mlady'])) {
        matches.push(route);
      }
      expect(matches.length).to.equal(2);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_HELLO_PARAM) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['hello', 'mlady']},
          {middleware: USE_SLASH, path: [], suffix: ['hello', 'mlady']},
          {middleware: USE_HELLO_PARAM, path: ['hello', 'mlady'], suffix: []},
          {endpoint: GET_HELLO_PARAM, path: ['hello', 'mlady'], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_ALL) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['hello', 'mlady']},
          {middleware: USE_SLASH, path: [], suffix: ['hello', 'mlady']},
          {router: CHILD2, path: [], suffix: ['hello', 'mlady']},
          {endpoint: GET_ALL, path: [], suffix: ['hello', 'mlady']}
        ]);
        return true;
      })).to.equal(true);
    });

    it('finds all routes for /potato/salad', function () {
      const matches = [];
      for (const route of tree.findRoutes(['potato', 'salad'])) {
        matches.push(route);
      }
      expect(matches.length).to.equal(3);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_POTATO_SALAD1) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['potato', 'salad']},
          {middleware: USE_SLASH, path: [], suffix: ['potato', 'salad']},
          {router: CHILD2, path: [], suffix: ['potato', 'salad']},
          {endpoint: GET_POTATO_SALAD1, path: ['potato', 'salad'], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_POTATO_SALAD2) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['potato', 'salad']},
          {middleware: USE_SLASH, path: [], suffix: ['potato', 'salad']},
          {router: CHILD2, path: [], suffix: ['potato', 'salad']},
          {endpoint: GET_POTATO_SALAD2, path: ['potato', 'salad'], suffix: []}
        ]);
        return true;
      })).to.equal(true);
      expect(matches.some(function (match) {
        const i = match.length - 1;
        if (match[i].endpoint !== GET_ALL) {
          return false;
        }
        expect(match).to.eql([
          {router: router, path: [], suffix: ['potato', 'salad']},
          {middleware: USE_SLASH, path: [], suffix: ['potato', 'salad']},
          {router: CHILD2, path: [], suffix: ['potato', 'salad']},
          {endpoint: GET_ALL, path: [], suffix: ['potato', 'salad']}
        ]);
        return true;
      })).to.equal(true);
    });
  });
});
