;(function(undefined) {
  'use strict';

  /**
   * Sigma Renderer Halo Utility
   * ================================
   *
   * The aim of this plugin is to display a circled halo behind specified nodes.
   *
   * Author: Sébastien Heymann (sheymann) for Linkurious
   * Version: 0.0.2
   */

  // Terminating if sigma were not to be found
  if (typeof sigma === 'undefined')
    throw new Error('sigma not in scope.');

  /**
   * Creates an array of unique values present in all provided arrays using
   * strict equality for comparisons, i.e. `===`.
   *
   * @see lodash
   * @param {...Array} [array] The arrays to inspect.
   * @return {Array} Returns an array of shared values.
   * @example
   *
   * _.intersection([1, 2, 3], [5, 2, 1, 4], [2, 1]);
   * // => [1, 2]
   */
  function intersection() {
    var args = [],
        argsIndex = -1,
        argsLength = arguments.length;

    while (++argsIndex < argsLength) {
      var value = arguments[argsIndex];
       args.push(value);
    }
    var array = args[0],
        index = -1,
        length = array ? array.length : 0,
        result = [];

    outer:
    while (++index < length) {
      value = array[index];
      if (result.indexOf(value) < 0) {
        var argsIndex = argsLength;
        while (--argsIndex) {
          if (args[argsIndex].indexOf(value) < 0) {
            continue outer;
          }
        }
        result.push(value);
      }
    }
    return result;
  }

  /**
   * Draw the specified circles in a given context.
   *
   * @param {array}   circles
   * @param {object}  context
   * @param {boolean} onlyStroke
   */
  function drawCircles(circles, context, onlyStroke) {
    for (var i = 0; i < circles.length; i++) {
      if (circles[i] == null) continue;

      context.beginPath();

      context.arc(
        circles[i].x,
        circles[i].y,
        circles[i].radius,
        0,
        Math.PI * 2,
        true
      );

      context.closePath();
      if (onlyStroke) {
        context.stroke();
      } else {
        context.fill();
      }
    }
  }

  /**
   * Avoid crossing strokes.
   *
   * @see http://vis4.net/blog/posts/clean-your-symbol-maps/
   * @param {array}  circles    The circles to cluster.
   * @param {number} margin     The minimal distance between the circle
   *                            and the points inside it.
   * @param {?number} maxRadius The max length of a radius
   * @return {array}            The clustered circles.
   */
  function clusterCircles(circles, margin, maxRadius) {
    maxRadius = maxRadius || Number.POSITIVE_INFINITY;

    // console.time('halo cluster');
    if (circles.length > 1) {
      var
        intersecting = true,
        centroid,
        d,
        points;

      while (intersecting) {
        intersecting = false;
        for (var i = 0; i < circles.length; i++) {
          if (circles[i] === null) continue;

          for (var j = i + 1; j < circles.length; j++) {
            if (circles[j] === null) continue;

            // distance between i-1 and i
            d = sigma.utils.getDistance(
              circles[i].x, circles[i].y, circles[j].x, circles[j].y
            );
            if (d < maxRadius && d < circles[i].radius + circles[j].radius) {
              intersecting = true;

              // Centers of the merged circles:
              points = [
                {x: circles[i].x, y: circles[i].y, radius: circles[i].radius},
                {x: circles[j].x, y: circles[j].y, radius: circles[j].radius}
              ];
              if (circles[i].points) {
                points = points.concat(circles[i].points);
              }
              if (circles[j].points) {
                points = points.concat(circles[j].points);
              }

              // Compute the centroid:
              centroid = {x: 0, y: 0};
              for (var p = 0; p < points.length; p++) {
                centroid.x += points[p].x;
                centroid.y += points[p].y;
              }
              centroid.x /= points.length;
              centroid.y /= points.length;

              // Compute radius:
              centroid.radius = Math.max.apply(null, points.map(function(point) {
                return margin + sigma.utils.getDistance(
                  centroid.x, centroid.y, point.x, point.y
                );
              }));

              // Merge circles
              circles.push({
                x: centroid.x,
                y: centroid.y,
                radius: centroid.radius,
                points: points
              });

              circles[i] = circles[j] = null;
              break; // exit for loop
            }
          }
        }
      }
    }
    // console.timeEnd('halo cluster');
    return circles;
  }


  // Main function
  function halo(params) {
    params = params || {};

    if (!this.domElements['background']) {
      this.initDOM('canvas', 'background');
      this.domElements['background'].width =
        this.container.offsetWidth;
      this.domElements['background'].height =
        this.container.offsetHeight;
      this.container.insertBefore(this.domElements['background'], this.container.firstChild);
    }

    var self = this,
        context = self.contexts.background,
        webgl = this instanceof sigma.renderers.webgl,
        ePrefix = self.options.prefix,
        nPrefix = webgl ? ePrefix.substr(5) : ePrefix,
        nHaloClustering = params.nodeHaloClustering || self.settings('nodeHaloClustering'),
        nHaloClusteringMaxRadius = params.nodeHaloClusteringMaxRadius || self.settings('nodeHaloClusteringMaxRadius'),
        nHaloColor = params.nodeHaloColor || self.settings('nodeHaloColor'),
        nHaloSize = params.nodeHaloSize || self.settings('nodeHaloSize'),
        nHaloStroke = params.nodeHaloStroke || self.settings('nodeHaloStroke'),
        nHaloStrokeColor = params.nodeHaloStrokeColor || self.settings('nodeHaloStrokeColor'),
        nHaloStrokeWidth = params.nodeHaloStrokeWidth || self.settings('nodeHaloStrokeWidth'),
        borderSize = self.settings('nodeBorderSize') || 0,
        outerBorderSize = self.settings('nodeOuterBorderSize') || 0,
        eHaloColor = params.edgeHaloColor || self.settings('edgeHaloColor'),
        eHaloSize = params.edgeHaloSize || self.settings('edgeHaloSize'),
        drawHalo = params.drawHalo || self.settings('drawHalo'),
        nodes = params.nodes || [],
        edges = params.edges || [],
        source,
        target,
        cp,
        sSize,
        sX,
        sY,
        tX,
        tY,
        margin,
        circles;

    if (!drawHalo) {
      return;
    }

    nodes = webgl ? nodes : intersection(params.nodes, self.nodesOnScreen);
    edges = webgl ? edges : intersection(params.edges, self.edgesOnScreen);

    // clear canvas
    context.clearRect(0, 0, context.canvas.width, context.canvas.height);

    context.save();

    // EDGES
    context.strokeStyle = eHaloColor;

    edges.forEach(function(edge) {
      source = self.graph.nodes(edge.source);
      target = self.graph.nodes(edge.target);

      context.lineWidth = (edge[ePrefix + 'size'] || 1) + eHaloSize;
      context.beginPath();

      cp = {};
      sSize = source[nPrefix + 'size'];
      sX = source[nPrefix + 'x'];
      sY = source[nPrefix + 'y'];
      tX = target[nPrefix + 'x'];
      tY = target[nPrefix + 'y'];

      context.moveTo(sX, sY);

      if (edge.type === 'curve' || edge.type === 'curvedArrow') {
        if (edge.source === edge.target) {
          cp = sigma.utils.getSelfLoopControlPoints(sX, sY, sSize);
          context.bezierCurveTo(cp.x1, cp.y1, cp.x2, cp.y2, tX, tY);
        }
        else {
          cp = sigma.utils.getQuadraticControlPoint(sX, sY, tX, tY, edge.cc);
          context.quadraticCurveTo(cp.x, cp.y, tX, tY);
        }
      }
      else {
        context.moveTo(sX, sY);
        context.lineTo(tX, tY);
      }
      context.stroke();

      context.closePath();
    });

    // NODES
    context.fillStyle = nHaloColor;

    if (nHaloStroke) {
      context.lineWidth = nHaloStrokeWidth;
      context.strokeStyle = nHaloStrokeColor;
    }

    margin = borderSize + outerBorderSize + nHaloSize;

    circles = nodes.filter(function(node) {
      return !node.hidden;
    })
    .map(function(node) {
      return {
        x: node[nPrefix + 'x'],
        y: node[nPrefix + 'y'],
        radius: node[nPrefix + 'size'] + margin,
      };
    });

    if (nHaloClustering) {
      // Avoid crossing strokes:
      circles = clusterCircles(circles, margin, nHaloClusteringMaxRadius);
    }
    if (nHaloStroke) {
      drawCircles(circles, context, true);
    }
    drawCircles(circles, context);

    context.restore();
  }

  // Extending canvas and webl renderers
  sigma.renderers.canvas.prototype.halo = halo;
  sigma.renderers.webgl.prototype.halo = halo;

  // TODO clear scene?
}).call(this);
