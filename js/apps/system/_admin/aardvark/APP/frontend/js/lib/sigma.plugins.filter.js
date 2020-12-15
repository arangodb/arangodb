;(function(undefined) {
  'use strict';

  if (typeof sigma === 'undefined')
    throw 'sigma is not declared';

  // Initialize package:
  sigma.utils.pkg('sigma.plugins');

  // Add custom graph methods:
  /**
   * This methods returns an array of nodes that are adjacent to a node.
   *
   * @param  {string} id The node id.
   * @return {array}     The array of adjacent nodes.
   */
  if (!sigma.classes.graph.hasMethod('adjacentNodes'))
    sigma.classes.graph.addMethod('adjacentNodes', function(id) {
      if (typeof id !== 'string')
        throw 'adjacentNodes: the node id must be a string.';

      var target,
          nodes = [];
      for(target in this.allNeighborsIndex[id]) {
        nodes.push(this.nodesIndex[target]);
      }
      return nodes;
    });

  /**
   * This methods returns an array of edges that are adjacent to a node.
   *
   * @param  {string} id The node id.
   * @return {array}     The array of adjacent edges.
   */
  if (!sigma.classes.graph.hasMethod('adjacentEdges'))
    sigma.classes.graph.addMethod('adjacentEdges', function(id) {
      if (typeof id !== 'string')
        throw 'adjacentEdges: the node id must be a string.';

      var a = this.allNeighborsIndex[id],
          eid,
          target,
          edges = [];
      for(target in a) {
        for(eid in a[target]) {
          edges.push(a[target][eid]);
        }
      }
      return edges;
    });

  /**
   * Sigma Filter
   * =============================
   *
   * @author Sébastien Heymann <seb@linkurio.us> (Linkurious)
   * @version 0.1
   */

   var _g = undefined,
       _s = undefined,
       _chain = [], // chain of wrapped filters
       _keysIndex = Object.create(null),
       Processors = {};   // available predicate processors


  /**
   * Library of processors
   * ------------------
   */

   /**
    *
    * @param  {function} fn The predicate.
    */
  Processors.nodes = function nodes(fn) {
    var n = _g.nodes(),
        ln = n.length,
        e = _g.edges(),
        le = e.length;

    // hide node, or keep former value
    while(ln--)
      n[ln].hidden = !fn.call(_g, n[ln]) || n[ln].hidden;

    while(le--)
      if (_g.nodes(e[le].source).hidden || _g.nodes(e[le].target).hidden)
        e[le].hidden = true;
  };

   /**
    *
    * @param  {function} fn The predicate.
    */
  Processors.edges = function edges(fn) {
    var e = _g.edges(),
        le = e.length;

    // hide edge, or keep former value
    while(le--)
      e[le].hidden = !fn.call(_g, e[le]) || e[le].hidden;
  };

   /**
    *
    * @param  {string} id The center node.
    */
  Processors.neighbors = function neighbors(id) {
    var n = _g.nodes(),
        ln = n.length,
        e = _g.edges(),
        le = e.length,
        neighbors = _g.adjacentNodes(id),
        nn = neighbors.length,
        no = {};

    while(nn--)
      no[neighbors[nn].id] = true;

    while(ln--)
      if (n[ln].id !== id && !(n[ln].id in no))
        n[ln].hidden = true;

    while(le--)
      if (_g.nodes(e[le].source).hidden || _g.nodes(e[le].target).hidden)
        e[le].hidden = true;
  };


  /**
   * This function adds a filter to the chain of filters.
   *
   * @param  {function} fn  The filter (i.e. predicate processor).
   * @param  {function} p   The predicate.
   * @param  {?string}  key The key to identify the filter.
   */
  function register(fn, p, key) {
    if (key != undefined && typeof key !== 'string')
      throw 'The filter key "'+ key.toString() +'" must be a string.';

    if (key != undefined && !key.length)
      throw 'The filter key must be a non-empty string.';

    if (typeof fn !== 'function')
      throw 'The predicate of key "'+ key +'" must be a function.';

    if ('undo' === key)
      throw '"undo" is a reserved key.';

    if (_keysIndex[key])
      throw 'The filter "' + key + '" already exists.';

    if (key)
      _keysIndex[key] = true;

    _chain.push({
      'key': key,
      'processor': fn,
      'predicate': p
    });
  };

  /**
   * This function removes a set of filters from the chain.
   *
   * @param {object} o The filter keys.
   */
  function unregister (o) {
    _chain = _chain.filter(function(a) {
      return !(a.key in o);
    });

    for(var key in o)
      delete _keysIndex[key];
  };




  /**
   * Filter Object
   * ------------------
   * @param  {sigma} s The related sigma instance.
   */
  function Filter(s) {
    _s = s;
    _g = s.graph;
  };


  /**
   * This method is used to filter the nodes. The method must be called with
   * the predicate, which is a function that takes a node as argument and
   * returns a boolean. It may take an identifier as argument to undo the
   * filter later. The method wraps the predicate into an anonymous function
   * that looks through each node in the graph. When executed, the anonymous
   * function hides the nodes that fail a truth test (predicate). The method
   * adds the anonymous function to the chain of filters. The filter is not
   * executed until the apply() method is called.
   *
   * > var filter = new sigma.plugins.filter(s);
   * > filter.nodesBy(function(n) {
   * >   return this.degree(n.id) > 0;
   * > }, 'degreeNotNull');
   *
   * @param  {function}             fn  The filter predicate.
   * @param  {?string}              key The key to identify the filter.
   * @return {sigma.plugins.filter}     Returns the instance.
   */
  Filter.prototype.nodesBy = function(fn, key) {
    // Wrap the predicate to be applied on the graph and add it to the chain.
    register(Processors.nodes, fn, key);

    return this;
  };

  /**
   * This method is used to filter the edges. The method must be called with
   * the predicate, which is a function that takes a node as argument and
   * returns a boolean. It may take an identifier as argument to undo the
   * filter later. The method wraps the predicate into an anonymous function
   * that looks through each edge in the graph. When executed, the anonymous
   * function hides the edges that fail a truth test (predicate). The method
   * adds the anonymous function to the chain of filters. The filter is not
   * executed until the apply() method is called.
   *
   * > var filter = new sigma.plugins.filter(s);
   * > filter.edgesBy(function(e) {
   * >   return e.size > 1;
   * > }, 'edgeSize');
   *
   * @param  {function}             fn  The filter predicate.
   * @param  {?string}              key The key to identify the filter.
   * @return {sigma.plugins.filter}     Returns the instance.
   */
  Filter.prototype.edgesBy = function(fn, key) {
    // Wrap the predicate to be applied on the graph and add it to the chain.
    register(Processors.edges, fn, key);

    return this;
  };

  /**
   * This method is used to filter the nodes which are not direct connections
   * of a given node. The method must be called with the node identifier. It
   * may take an identifier as argument to undo the filter later. The filter
   * is not executed until the apply() method is called.
   *
   * > var filter = new sigma.plugins.filter(s);
   * > filter.neighborsOf('n0');
   *
   * @param  {string}               id  The node id.
   * @param  {?string}              key The key to identify the filter.
   * @return {sigma.plugins.filter}     Returns the instance.
   */
  Filter.prototype.neighborsOf = function(id, key) {
    if (typeof id !== 'string')
      throw 'The node id "'+ id.toString() +'" must be a string.';
    if (!id.length)
      throw 'The node id must be a non-empty string.';

    // Wrap the predicate to be applied on the graph and add it to the chain.
    register(Processors.neighbors, id, key);

    return this;
  };

  /**
   * This method is used to execute the chain of filters and to refresh the
   * display.
   *
   * > var filter = new sigma.plugins.filter(s);
   * > filter
   * >   .nodesBy(function(n) {
   * >     return this.degree(n.id) > 0;
   * >   }, 'degreeNotNull')
   * >   .apply();
   *
   * @return {sigma.plugins.filter}      Returns the instance.
   */
  Filter.prototype.apply = function() {
    for (var i = 0, len = _chain.length; i < len; ++i) {
      _chain[i].processor(_chain[i].predicate);
    };

    if (_chain[0] && 'undo' === _chain[0].key) {
      _chain.shift();
    }

    _s.refresh();

    return this;
  };

  /**
   * This method undoes one or several filters, depending on how it is called.
   *
   * To undo all filters, call "undo" without argument. To undo a specific
   * filter, call it with the key of the filter. To undo multiple filters, call
   * it with an array of keys or multiple arguments, and it will undo each
   * filter, in the same order. The undo is not executed until the apply()
   * method is called. For instance:
   *
   * > var filter = new sigma.plugins.filter(s);
   * > filter
   * >   .nodesBy(function(n) {
   * >     return this.degree(n.id) > 0;
   * >   }, 'degreeNotNull');
   * >   .edgesBy(function(e) {
   * >     return e.size > 1;
   * >   }, 'edgeSize')
   * >   .undo();
   *
   * Other examples:
   * > filter.undo();
   * > filter.undo('myfilter');
   * > filter.undo(['myfilter1', 'myfilter2']);
   * > filter.undo('myfilter1', 'myfilter2');
   *
   * @param  {?(string|array|*string))} v Eventually one key, an array of keys.
   * @return {sigma.plugins.filter}       Returns the instance.
   */
  Filter.prototype.undo = function(v) {
    var q = Object.create(null),
        la = arguments.length;

    // find removable filters
    if (la === 1) {
      if (Object.prototype.toString.call(v) === '[object Array]')
        for (var i = 0, len = v.length; i < len; i++)
          q[v[i]] = true;

      else // 1 filter key
        q[v] = true;

    } else if (la > 1) {
      for (var i = 0; i < la; i++)
        q[arguments[i]] = true;
    }
    else
      this.clear();

    unregister(q);

    function processor() {
      var n = _g.nodes(),
          ln = n.length,
          e = _g.edges(),
          le = e.length;

      while(ln--)
        n[ln].hidden = false;

      while(le--)
        e[le].hidden = false;
    };

    _chain.unshift({
      'key': 'undo',
      'processor': processor
    });

    return this;
  };

  // fast deep copy function
  function deepCopy(o) {
    var copy = Object.create(null);
    for (var i in o) {
      if (typeof o[i] === "object" && o[i] !== null) {
        copy[i] = deepCopy(o[i]);
      }
      else if (typeof o[i] === "function" && o[i] !== null) {
        // clone function:
        eval(" copy[i] = " +  o[i].toString());
        //copy[i] = o[i].bind(_g);
      }

      else
        copy[i] = o[i];
    }
    return copy;
  };

  function cloneChain(chain) {
    // Clone the array of filters:
    var copy = chain.slice(0);
    for (var i = 0, len = copy.length; i < len; i++) {
      copy[i] = deepCopy(copy[i]);
      if (typeof copy[i].processor === "function")
        copy[i].processor = 'filter.processors.' + copy[i].processor.name;
    };
    return copy;
  }

  /**
   * This method is used to empty the chain of filters.
   * Prefer the undo() method to reset filters.
   *
   * > var filter = new sigma.plugins.filter(s);
   * > filter.clear();
   *
   * @return {sigma.plugins.filter} Returns the instance.
   */
  Filter.prototype.clear = function() {
    _chain.length = 0; // clear the array
    _keysIndex = Object.create(null);
    return this;
  };

  /**
   * This method clones the filter chain and return the copy.
   *
   * > var filter = new sigma.plugins.filter(s);
   * > var chain = filter.export();
   *
   * @return {object}   The cloned chain of filters.
   */
  Filter.prototype.export = function() {
    var c = cloneChain(_chain);
    return c;
  };

  /**
   * This method sets the chain of filters with the specified chain.
   *
   * > var filter = new sigma.plugins.filter(s);
   * > var chain = [
   * >   {
   * >     key: 'my-filter',
   * >     predicate: function(n) {...},
   * >     processor: 'filter.processors.nodes'
   * >   }, ...
   * > ];
   * > filter.import(chain);
   *
   * @param {array} chain The chain of filters.
   * @return {sigma.plugins.filter} Returns the instance.
   */
  Filter.prototype.import = function(chain) {
    if (chain === undefined)
      throw 'Wrong arguments.';

    if (Object.prototype.toString.call(chain) !== '[object Array]')
      throw 'The chain" must be an array.';

    var copy = cloneChain(chain);

    for (var i = 0, len = copy.length; i < len; i++) {
      if (copy[i].predicate === undefined || copy[i].processor === undefined)
        throw 'Wrong arguments.';

      if (copy[i].key != undefined && typeof copy[i].key !== 'string')
        throw 'The filter key "'+ copy[i].key.toString() +'" must be a string.';

      if (typeof copy[i].predicate !== 'function')
        throw 'The predicate of key "'+ copy[i].key +'" must be a function.';

      if (typeof copy[i].processor !== 'string')
        throw 'The processor of key "'+ copy[i].key +'" must be a string.';

      // Replace the processor name by the corresponding function:
      switch(copy[i].processor) {
        case 'filter.processors.nodes':
          copy[i].processor = Processors.nodes;
          break;
        case 'filter.processors.edges':
          copy[i].processor = Processors.edges;
          break;
        case 'filter.processors.neighbors':
          copy[i].processor = Processors.neighbors;
          break;
        default:
          throw 'Unknown processor ' + copy[i].processor;
      }
    };

    _chain = copy;

    return this;
  };


  /**
   * Interface
   * ------------------
   *
   * > var filter = new sigma.plugins.filter(s);
   */
  var filter = null;

  /**
   * @param  {sigma} s The related sigma instance.
   */
  sigma.plugins.filter = function(s) {
    // Create filter if undefined
    if (!filter) {
      filter = new Filter(s);
    }
    return filter;
  };

}).call(this);
