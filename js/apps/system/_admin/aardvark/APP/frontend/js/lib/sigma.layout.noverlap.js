;(function(undefined) {
  'use strict';

  if (typeof sigma === 'undefined')
    throw new Error('sigma is not declared');

  // Initialize package:
  sigma.utils.pkg('sigma.layout.noverlap');

  /**
   * Noverlap Layout
   * ===============================
   *
   * Author: @apitts / Andrew Pitts
   * Algorithm: @jacomyma / Mathieu Jacomy (originally contributed to Gephi and ported to sigma.js under the MIT license by @andpitts with permission)
   * Acknowledgement: @sheyman / Sébastien Heymann (some inspiration has been taken from other MIT licensed layout algorithms authored by @sheyman)
   * Version: 0.1
   */

  var settings = {
    speed: 3,
    scaleNodes: 1.2,
    nodeMargin: 5.0,
    gridSize: 20,
    permittedExpansion: 1.1,
    rendererIndex: 0,
    maxIterations: 500
  };

  var _instance = {};

  /**
   * Event emitter Object
   * ------------------
   */
  var _eventEmitter = {};

   /**
   * Noverlap Object
   * ------------------
   */
  function Noverlap() {
    var self = this;

    this.init = function (sigInst, options) {
      options = options || {};

      // Properties
      this.sigInst = sigInst;
      this.config = sigma.utils.extend(options, settings);
      this.easing = options.easing;
      this.duration = options.duration;

      if (options.nodes) {
        this.nodes = options.nodes;
        delete options.nodes;
      }

      if (!sigma.plugins || typeof sigma.plugins.animate === 'undefined') {
        throw new Error('sigma.plugins.animate is not declared');
      }

      // State
      this.running = false;
    };

    /**
     * Single layout iteration.
     */
    this.atomicGo = function () {
      if (!this.running || this.iterCount < 1) return false;

      var nodes = this.nodes || this.sigInst.graph.nodes(),
          nodesCount = nodes.length,
          i,
          n,
          n1,
          n2,
          xmin = Infinity,
          xmax = -Infinity,
          ymin = Infinity,
          ymax = -Infinity,
          xwidth,
          yheight,
          xcenter,
          ycenter,
          grid,
          row,
          col,
          minXBox,
          maxXBox,
          minYBox,
          maxYBox,
          adjacentNodes,
          subRow,
          subCol,
          nxmin,
          nxmax,
          nymin,
          nymax;

      this.iterCount--;
      this.running = false;

      for (i=0; i < nodesCount; i++) {
        n = nodes[i];
        n.dn.dx = 0;
        n.dn.dy = 0;

        //Find the min and max for both x and y across all nodes
        xmin = Math.min(xmin, n.dn_x - (n.dn_size*self.config.scaleNodes + self.config.nodeMargin) );
        xmax = Math.max(xmax, n.dn_x + (n.dn_size*self.config.scaleNodes + self.config.nodeMargin) );
        ymin = Math.min(ymin, n.dn_y - (n.dn_size*self.config.scaleNodes + self.config.nodeMargin) );
        ymax = Math.max(ymax, n.dn_y + (n.dn_size*self.config.scaleNodes + self.config.nodeMargin) );

      }

      xwidth = xmax - xmin;
      yheight = ymax - ymin;
      xcenter = (xmin + xmax) / 2;
      ycenter = (ymin + ymax) / 2;
      xmin = xcenter - self.config.permittedExpansion*xwidth / 2;
      xmax = xcenter + self.config.permittedExpansion*xwidth / 2;
      ymin = ycenter - self.config.permittedExpansion*yheight / 2;
      ymax = ycenter + self.config.permittedExpansion*yheight / 2;

      grid = {}; //An object of objects where grid[row][col] is an array of node ids representing nodes that fall in that grid. Nodes can fall in more than one grid

      for(row = 0; row < self.config.gridSize; row++) {
        grid[row] = {};
        for(col = 0; col < self.config.gridSize; col++) {
          grid[row][col] = [];
        }
      }

      //Place nodes in grid
      for (i=0; i < nodesCount; i++) {
        n = nodes[i];

        nxmin = n.dn_x - (n.dn_size*self.config.scaleNodes + self.config.nodeMargin);
        nxmax = n.dn_x + (n.dn_size*self.config.scaleNodes + self.config.nodeMargin);
        nymin = n.dn_y - (n.dn_size*self.config.scaleNodes + self.config.nodeMargin);
        nymax = n.dn_y + (n.dn_size*self.config.scaleNodes + self.config.nodeMargin);

        minXBox = Math.floor(self.config.gridSize* (nxmin - xmin) / (xmax - xmin) );
        maxXBox = Math.floor(self.config.gridSize* (nxmax - xmin) / (xmax - xmin) );
        minYBox = Math.floor(self.config.gridSize* (nymin - ymin) / (ymax - ymin) );
        maxYBox = Math.floor(self.config.gridSize* (nymax - ymin) / (ymax - ymin) );
        for(col = minXBox; col <= maxXBox; col++) {
          for(row = minYBox; row <= maxYBox; row++) {
            grid[row][col].push(n.id);
          }
        }
      }


      adjacentNodes = {}; //An object that stores the node ids of adjacent nodes (either in same grid box or adjacent grid box) for all nodes

      for(row = 0; row < self.config.gridSize; row++) {
        for(col = 0; col < self.config.gridSize; col++) {
          grid[row][col].forEach(function(nodeId) {
            if(!adjacentNodes[nodeId]) {
              adjacentNodes[nodeId] = [];
            }
            for(subRow = Math.max(0, row - 1); subRow <= Math.min(row + 1, self.config.gridSize - 1); subRow++) {
              for(subCol = Math.max(0, col - 1); subCol <= Math.min(col + 1,  self.config.gridSize - 1); subCol++) {
                grid[subRow][subCol].forEach(function(subNodeId) {
                  if(subNodeId !== nodeId && adjacentNodes[nodeId].indexOf(subNodeId) === -1) {
                    adjacentNodes[nodeId].push(subNodeId);
                  }
                });
              }
            }
          });
        }
      }

      //If two nodes overlap then repulse them
      for (i=0; i < nodesCount; i++) {
        n1 = nodes[i];
        adjacentNodes[n1.id].forEach(function(nodeId) {
          var n2 = self.sigInst.graph.nodes(nodeId);
          var xDist = n2.dn_x - n1.dn_x;
          var yDist = n2.dn_y - n1.dn_y;
          var dist = Math.sqrt(xDist*xDist + yDist*yDist);
          var collision = (dist < ((n1.dn_size*self.config.scaleNodes + self.config.nodeMargin) + (n2.dn_size*self.config.scaleNodes + self.config.nodeMargin)));
          if(collision) {
            self.running = true;
            if(dist > 0) {
              n2.dn.dx += xDist / dist * (1 + n1.dn_size);
              n2.dn.dy += yDist / dist * (1 + n1.dn_size);
            } else {
              n2.dn.dx += xwidth * 0.01 * (0.5 - Math.random());
              n2.dn.dy += yheight * 0.01 * (0.5 - Math.random());
            }
          }
        });
      }

      for (i=0; i < nodesCount; i++) {
        n = nodes[i];
        if(!n.fixed) {
          n.dn_x = n.dn_x + n.dn.dx * 0.1 * self.config.speed;
          n.dn_y = n.dn_y + n.dn.dy * 0.1 * self.config.speed;
        }
      }

      if(this.running && this.iterCount < 1) {
        this.running = false;
      }

      return this.running;
    };

    this.go = function () {
      this.iterCount = this.config.maxIterations;

      while (this.running) {
        this.atomicGo();
      };

      this.stop();
    };

    this.start = function() {
      if (this.running) return;

      var nodes = this.sigInst.graph.nodes();

      var prefix = this.sigInst.renderers[self.config.rendererIndex].options.prefix;

      this.running = true;

      // Init nodes
      for (var i = 0; i < nodes.length; i++) {
        nodes[i].dn_x = nodes[i][prefix + 'x'];
        nodes[i].dn_y = nodes[i][prefix + 'y'];
        nodes[i].dn_size = nodes[i][prefix + 'size'];
        nodes[i].dn = {
          dx: 0,
          dy: 0
        };
      }
      _eventEmitter[self.sigInst.id].dispatchEvent('start');
      this.go();
    };

    this.stop = function() {
      var nodes = this.sigInst.graph.nodes();

      this.running = false;

      if (this.easing) {
        _eventEmitter[self.sigInst.id].dispatchEvent('interpolate');
        sigma.plugins.animate(
          self.sigInst,
          {
            x: 'dn_x',
            y: 'dn_y'
          },
          {
            easing: self.easing,
            onComplete: function() {
              self.sigInst.refresh();
              for (var i = 0; i < nodes.length; i++) {
                nodes[i].dn = null;
                nodes[i].dn_x = null;
                nodes[i].dn_y = null;
              }
              _eventEmitter[self.sigInst.id].dispatchEvent('stop');
            },
            duration: self.duration
          }
        );
      }
      else {
        // Apply changes
        for (var i = 0; i < nodes.length; i++) {
          nodes[i].x = nodes[i].dn_x;
          nodes[i].y = nodes[i].dn_y;
        }

        this.sigInst.refresh();

        for (var i = 0; i < nodes.length; i++) {
          nodes[i].dn = null;
          nodes[i].dn_x = null;
          nodes[i].dn_y = null;
        }
        _eventEmitter[self.sigInst.id].dispatchEvent('stop');
      }
    };

    this.kill = function() {
      this.sigInst = null;
      this.config = null;
      this.easing = null;
    };
  };

  /**
   * Interface
   * ----------
   */

  /**
   * Configure the layout algorithm.

   * Recognized options:
   * **********************
   * Here is the exhaustive list of every accepted parameter in the settings
   * object:
   *
   *   {?number}            speed               A larger value increases the convergence speed at the cost of precision
   *   {?number}            scaleNodes          The ratio to scale nodes by - a larger ratio will lead to more space around larger nodes
   *   {?number}            nodeMargin          A fixed margin to apply around nodes regardless of size
   *   {?number}            maxIterations       The maximum number of iterations to perform before the layout completes.
   *   {?integer}           gridSize            The number of rows and columns to use when partioning nodes into a grid for efficient computation
   *   {?number}            permittedExpansion  A permitted expansion factor to the overall size of the network applied at each iteration
   *   {?integer}           rendererIndex       The index of the renderer to use for node co-ordinates. Defaults to zero.
   *   {?(function|string)} easing              Either the name of an easing in the sigma.utils.easings package or a function. If not specified, the
   *                                            quadraticInOut easing from this package will be used instead.
   *   {?number}            duration            The duration of the animation. If not specified, the "animationsTime" setting value of the sigma instance will be used instead.
   *
   *
   * @param  {object} config  The optional configuration object.
   *
   * @return {sigma.classes.dispatcher} Returns an event emitter.
   */
  sigma.prototype.configNoverlap = function(config) {

    var sigInst = this;

    if (!config) throw new Error('Missing argument: "config"');

    // Create instance if undefined
    if (!_instance[sigInst.id]) {
      _instance[sigInst.id] = new Noverlap();

      _eventEmitter[sigInst.id] = {};
      sigma.classes.dispatcher.extend(_eventEmitter[sigInst.id]);

      // Binding on kill to clear the references
      sigInst.bind('kill', function() {
        _instance[sigInst.id].kill();
        _instance[sigInst.id] = null;
        _eventEmitter[sigInst.id] = null;
      });
    }

    _instance[sigInst.id].init(sigInst, config);

    return _eventEmitter[sigInst.id];
  };

  /**
   * Start the layout algorithm. It will use the existing configuration if no
   * new configuration is passed.

   * Recognized options:
   * **********************
   * Here is the exhaustive list of every accepted parameter in the settings
   * object
   *
   *   {?number}            speed               A larger value increases the convergence speed at the cost of precision
   *   {?number}            scaleNodes          The ratio to scale nodes by - a larger ratio will lead to more space around larger nodes
   *   {?number}            nodeMargin          A fixed margin to apply around nodes regardless of size
   *   {?number}            maxIterations       The maximum number of iterations to perform before the layout completes.
   *   {?integer}           gridSize            The number of rows and columns to use when partioning nodes into a grid for efficient computation
   *   {?number}            permittedExpansion  A permitted expansion factor to the overall size of the network applied at each iteration
   *   {?integer}           rendererIndex       The index of the renderer to use for node co-ordinates. Defaults to zero.
   *   {?(function|string)} easing              Either the name of an easing in the sigma.utils.easings package or a function. If not specified, the
   *                                            quadraticInOut easing from this package will be used instead.
   *   {?number}            duration            The duration of the animation. If not specified, the "animationsTime" setting value of the sigma instance will be used instead.
   *
   *
   *
   * @param  {object} config  The optional configuration object.
   *
   * @return {sigma.classes.dispatcher} Returns an event emitter.
   */

  sigma.prototype.startNoverlap = function(config) {

    var sigInst = this;

    if (config) {
      this.configNoverlap(sigInst, config);
    }

    _instance[sigInst.id].start();

    return _eventEmitter[sigInst.id];
  };

  /**
   * Returns true if the layout has started and is not completed.
   *
   * @return {boolean}
   */
  sigma.prototype.isNoverlapRunning = function() {

    var sigInst = this;

    return !!_instance[sigInst.id] && _instance[sigInst.id].running;
  };

}).call(this);