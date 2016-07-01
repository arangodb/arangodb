;(function(undefined) {
  'use strict';

  /**
   * Sigma ForceAtlas2.5 Webworker
   * ==============================
   *
   * Author: Guillaume Plique (Yomguithereal)
   * Algorithm author: Mathieu Jacomy @ Sciences Po Medialab & WebAtlas
   * Version: 1.0.3
   */

  var _root = this,
      inWebWorker = !('document' in _root);

  /**
   * Worker Function Wrapper
   * ------------------------
   *
   * The worker has to be wrapped into a single stringified function
   * to be passed afterwards as a BLOB object to the supervisor.
   */
  var Worker = function(undefined) {
    'use strict';

    /**
     * Worker settings and properties
     */
    var W = {

      // Properties
      ppn: 10,
      ppe: 3,
      ppr: 9,
      maxForce: 10,
      iterations: 0,
      converged: false,

      // Possible to change through config
      settings: {
        linLogMode: false,
        outboundAttractionDistribution: false,
        adjustSizes: false,
        edgeWeightInfluence: 0,
        scalingRatio: 1,
        strongGravityMode: false,
        gravity: 1,
        slowDown: 1,
        barnesHutOptimize: false,
        barnesHutTheta: 0.5,
        startingIterations: 1,
        iterationsPerRender: 1
      }
    };

    var NodeMatrix,
        EdgeMatrix,
        RegionMatrix;

    /**
     * Helpers
     */
    function extend() {
      var i,
          k,
          res = {},
          l = arguments.length;

      for (i = l - 1; i >= 0; i--)
        for (k in arguments[i])
          res[k] = arguments[i][k];
      return res;
    }

    function __emptyObject(obj) {
      var k;

      for (k in obj)
        if (!('hasOwnProperty' in obj) || obj.hasOwnProperty(k))
          delete obj[k];

      return obj;
    }

    /**
     * Matrices properties accessors
     */
    var nodeProperties = {
      x: 0,
      y: 1,
      dx: 2,
      dy: 3,
      old_dx: 4,
      old_dy: 5,
      mass: 6,
      convergence: 7,
      size: 8,
      fixed: 9
    };

    var edgeProperties = {
      source: 0,
      target: 1,
      weight: 2
    };

    var regionProperties = {
      node: 0,
      centerX: 1,
      centerY: 2,
      size: 3,
      nextSibling: 4,
      firstChild: 5,
      mass: 6,
      massCenterX: 7,
      massCenterY: 8
    };

    function np(i, p) {

      // DEBUG: safeguards
      if ((i % W.ppn) !== 0)
        throw 'np: non correct (' + i + ').';
      if (i !== parseInt(i))
        throw 'np: non int.';

      if (p in nodeProperties)
        return i + nodeProperties[p];
      else
        throw 'ForceAtlas2.Worker - ' +
              'Inexistant node property given (' + p + ').';
    }

    function ep(i, p) {

      // DEBUG: safeguards
      if ((i % W.ppe) !== 0)
        throw 'ep: non correct (' + i + ').';
      if (i !== parseInt(i))
        throw 'ep: non int.';

      if (p in edgeProperties)
        return i + edgeProperties[p];
      else
        throw 'ForceAtlas2.Worker - ' +
              'Inexistant edge property given (' + p + ').';
    }

    function rp(i, p) {

      // DEBUG: safeguards
      if ((i % W.ppr) !== 0)
        throw 'rp: non correct (' + i + ').';
      if (i !== parseInt(i))
        throw 'rp: non int.';

      if (p in regionProperties)
        return i + regionProperties[p];
      else
        throw 'ForceAtlas2.Worker - ' +
              'Inexistant region property given (' + p + ').';
    }

    // DEBUG
    function nan(v) {
      if (isNaN(v))
        throw 'NaN alert!';
    }


    /**
     * Algorithm initialization
     */

    function init(nodes, edges, config) {
      config = config || {};
      var i, l;

      // Matrices
      NodeMatrix = nodes;
      EdgeMatrix = edges;

      // Length
      W.nodesLength = NodeMatrix.length;
      W.edgesLength = EdgeMatrix.length;

      // Merging configuration
      configure(config);
    }

    function configure(o) {
      W.settings = extend(o, W.settings);
    }

    /**
     * Algorithm pass
     */

    // MATH: get distances stuff and power 2 issues
    function pass() {
      var a, i, j, l, r, n, n1, n2, e, w, g, k, m;

      var outboundAttCompensation,
          coefficient,
          xDist,
          yDist,
          ewc,
          mass,
          distance,
          size,
          factor;

      // 1) Initializing layout data
      //-----------------------------

      // Resetting positions & computing max values
      for (n = 0; n < W.nodesLength; n += W.ppn) {
        NodeMatrix[np(n, 'old_dx')] = NodeMatrix[np(n, 'dx')];
        NodeMatrix[np(n, 'old_dy')] = NodeMatrix[np(n, 'dy')];
        NodeMatrix[np(n, 'dx')] = 0;
        NodeMatrix[np(n, 'dy')] = 0;
      }

      // If outbound attraction distribution, compensate
      if (W.settings.outboundAttractionDistribution) {
        outboundAttCompensation = 0;
        for (n = 0; n < W.nodesLength; n += W.ppn) {
          outboundAttCompensation += NodeMatrix[np(n, 'mass')];
        }

        outboundAttCompensation /= W.nodesLength;
      }


      // 1.bis) Barnes-Hut computation
      //------------------------------

      if (W.settings.barnesHutOptimize) {

        var minX = Infinity,
            maxX = -Infinity,
            minY = Infinity,
            maxY = -Infinity,
            q, q0, q1, q2, q3;

        // Setting up
        // RegionMatrix = new Float32Array(W.nodesLength / W.ppn * 4 * W.ppr);
        RegionMatrix = [];

        // Computing min and max values
        for (n = 0; n < W.nodesLength; n += W.ppn) {
          minX = Math.min(minX, NodeMatrix[np(n, 'x')]);
          maxX = Math.max(maxX, NodeMatrix[np(n, 'x')]);
          minY = Math.min(minY, NodeMatrix[np(n, 'y')]);
          maxY = Math.max(maxY, NodeMatrix[np(n, 'y')]);
        }

        // Build the Barnes Hut root region
        RegionMatrix[rp(0, 'node')] = -1;
        RegionMatrix[rp(0, 'centerX')] = (minX + maxX) / 2;
        RegionMatrix[rp(0, 'centerY')] = (minY + maxY) / 2;
        RegionMatrix[rp(0, 'size')] = Math.max(maxX - minX, maxY - minY);
        RegionMatrix[rp(0, 'nextSibling')] = -1;
        RegionMatrix[rp(0, 'firstChild')] = -1;
        RegionMatrix[rp(0, 'mass')] = 0;
        RegionMatrix[rp(0, 'massCenterX')] = 0;
        RegionMatrix[rp(0, 'massCenterY')] = 0;

        // Add each node in the tree
        l = 1;
        for (n = 0; n < W.nodesLength; n += W.ppn) {

          // Current region, starting with root
          r = 0;

          while (true) {
            // Are there sub-regions?

            // We look at first child index
            if (RegionMatrix[rp(r, 'firstChild')] >= 0) {

              // There are sub-regions

              // We just iterate to find a "leave" of the tree
              // that is an empty region or a region with a single node
              // (see next case)

              // Find the quadrant of n
              if (NodeMatrix[np(n, 'x')] < RegionMatrix[rp(r, 'centerX')]) {

                if (NodeMatrix[np(n, 'y')] < RegionMatrix[rp(r, 'centerY')]) {

                  // Top Left quarter
                  q = RegionMatrix[rp(r, 'firstChild')];
                }
                else {

                  // Bottom Left quarter
                  q = RegionMatrix[rp(r, 'firstChild')] + W.ppr;
                }
              }
              else {
                if (NodeMatrix[np(n, 'y')] < RegionMatrix[rp(r, 'centerY')]) {

                  // Top Right quarter
                  q = RegionMatrix[rp(r, 'firstChild')] + W.ppr * 2;
                }
                else {

                  // Bottom Right quarter
                  q = RegionMatrix[rp(r, 'firstChild')] + W.ppr * 3;
                }
              }

              // Update center of mass and mass (we only do it for non-leave regions)
              RegionMatrix[rp(r, 'massCenterX')] =
                (RegionMatrix[rp(r, 'massCenterX')] * RegionMatrix[rp(r, 'mass')] +
                 NodeMatrix[np(n, 'x')] * NodeMatrix[np(n, 'mass')]) /
                (RegionMatrix[rp(r, 'mass')] + NodeMatrix[np(n, 'mass')]);

              RegionMatrix[rp(r, 'massCenterY')] =
                (RegionMatrix[rp(r, 'massCenterY')] * RegionMatrix[rp(r, 'mass')] +
                 NodeMatrix[np(n, 'y')] * NodeMatrix[np(n, 'mass')]) /
                (RegionMatrix[rp(r, 'mass')] + NodeMatrix[np(n, 'mass')]);

              RegionMatrix[rp(r, 'mass')] += NodeMatrix[np(n, 'mass')];

              // Iterate on the right quadrant
              r = q;
              continue;
            }
            else {

              // There are no sub-regions: we are in a "leave"

              // Is there a node in this leave?
              if (RegionMatrix[rp(r, 'node')] < 0) {

                // There is no node in region:
                // we record node n and go on
                RegionMatrix[rp(r, 'node')] = n;
                break;
              }
              else {

                // There is a node in this region

                // We will need to create sub-regions, stick the two
                // nodes (the old one r[0] and the new one n) in two
                // subregions. If they fall in the same quadrant,
                // we will iterate.

                // Create sub-regions
                RegionMatrix[rp(r, 'firstChild')] = l * W.ppr;
                w = RegionMatrix[rp(r, 'size')] / 2;  // new size (half)

                // NOTE: we use screen coordinates
                // from Top Left to Bottom Right

                // Top Left sub-region
                g = RegionMatrix[rp(r, 'firstChild')];

                RegionMatrix[rp(g, 'node')] = -1;
                RegionMatrix[rp(g, 'centerX')] = RegionMatrix[rp(r, 'centerX')] - w;
                RegionMatrix[rp(g, 'centerY')] = RegionMatrix[rp(r, 'centerY')] - w;
                RegionMatrix[rp(g, 'size')] = w;
                RegionMatrix[rp(g, 'nextSibling')] = g + W.ppr;
                RegionMatrix[rp(g, 'firstChild')] = -1;
                RegionMatrix[rp(g, 'mass')] = 0;
                RegionMatrix[rp(g, 'massCenterX')] = 0;
                RegionMatrix[rp(g, 'massCenterY')] = 0;

                // Bottom Left sub-region
                g += W.ppr;
                RegionMatrix[rp(g, 'node')] = -1;
                RegionMatrix[rp(g, 'centerX')] = RegionMatrix[rp(r, 'centerX')] - w;
                RegionMatrix[rp(g, 'centerY')] = RegionMatrix[rp(r, 'centerY')] + w;
                RegionMatrix[rp(g, 'size')] = w;
                RegionMatrix[rp(g, 'nextSibling')] = g + W.ppr;
                RegionMatrix[rp(g, 'firstChild')] = -1;
                RegionMatrix[rp(g, 'mass')] = 0;
                RegionMatrix[rp(g, 'massCenterX')] = 0;
                RegionMatrix[rp(g, 'massCenterY')] = 0;

                // Top Right sub-region
                g += W.ppr;
                RegionMatrix[rp(g, 'node')] = -1;
                RegionMatrix[rp(g, 'centerX')] = RegionMatrix[rp(r, 'centerX')] + w;
                RegionMatrix[rp(g, 'centerY')] = RegionMatrix[rp(r, 'centerY')] - w;
                RegionMatrix[rp(g, 'size')] = w;
                RegionMatrix[rp(g, 'nextSibling')] = g + W.ppr;
                RegionMatrix[rp(g, 'firstChild')] = -1;
                RegionMatrix[rp(g, 'mass')] = 0;
                RegionMatrix[rp(g, 'massCenterX')] = 0;
                RegionMatrix[rp(g, 'massCenterY')] = 0;

                // Bottom Right sub-region
                g += W.ppr;
                RegionMatrix[rp(g, 'node')] = -1;
                RegionMatrix[rp(g, 'centerX')] = RegionMatrix[rp(r, 'centerX')] + w;
                RegionMatrix[rp(g, 'centerY')] = RegionMatrix[rp(r, 'centerY')] + w;
                RegionMatrix[rp(g, 'size')] = w;
                RegionMatrix[rp(g, 'nextSibling')] = RegionMatrix[rp(r, 'nextSibling')];
                RegionMatrix[rp(g, 'firstChild')] = -1;
                RegionMatrix[rp(g, 'mass')] = 0;
                RegionMatrix[rp(g, 'massCenterX')] = 0;
                RegionMatrix[rp(g, 'massCenterY')] = 0;

                l += 4;

                // Now the goal is to find two different sub-regions
                // for the two nodes: the one previously recorded (r[0])
                // and the one we want to add (n)

                // Find the quadrant of the old node
                if (NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'x')] < RegionMatrix[rp(r, 'centerX')]) {
                  if (NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'y')] < RegionMatrix[rp(r, 'centerY')]) {

                    // Top Left quarter
                    q = RegionMatrix[rp(r, 'firstChild')];
                  }
                  else {

                    // Bottom Left quarter
                    q = RegionMatrix[rp(r, 'firstChild')] + W.ppr;
                  }
                }
                else {
                  if (NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'y')] < RegionMatrix[rp(r, 'centerY')]) {

                    // Top Right quarter
                    q = RegionMatrix[rp(r, 'firstChild')] + W.ppr * 2;
                  }
                  else {

                    // Bottom Right quarter
                    q = RegionMatrix[rp(r, 'firstChild')] + W.ppr * 3;
                  }
                }

                // We remove r[0] from the region r, add its mass to r and record it in q
                RegionMatrix[rp(r, 'mass')] = NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'mass')];
                RegionMatrix[rp(r, 'massCenterX')] = NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'x')];
                RegionMatrix[rp(r, 'massCenterY')] = NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'y')];

                RegionMatrix[rp(q, 'node')] = RegionMatrix[rp(r, 'node')];
                RegionMatrix[rp(r, 'node')] = -1;

                // Find the quadrant of n
                if (NodeMatrix[np(n, 'x')] < RegionMatrix[rp(r, 'centerX')]) {
                  if (NodeMatrix[np(n, 'y')] < RegionMatrix[rp(r, 'centerY')]) {

                    // Top Left quarter
                    q2 = RegionMatrix[rp(r, 'firstChild')];
                  }
                  else {
                    // Bottom Left quarter
                    q2 = RegionMatrix[rp(r, 'firstChild')] + W.ppr;
                  }
                }
                else {
                  if(NodeMatrix[np(n, 'y')] < RegionMatrix[rp(r, 'centerY')]) {

                    // Top Right quarter
                    q2 = RegionMatrix[rp(r, 'firstChild')] + W.ppr * 2;
                  }
                  else {

                    // Bottom Right quarter
                    q2 = RegionMatrix[rp(r, 'firstChild')] + W.ppr * 3;
                  }
                }

                if (q === q2) {

                  // If both nodes are in the same quadrant,
                  // we have to try it again on this quadrant
                  r = q;
                  continue;
                }

                // If both quadrants are different, we record n
                // in its quadrant
                RegionMatrix[rp(q2, 'node')] = n;
                break;
              }
            }
          }
        }
      }


      // 2) Repulsion
      //--------------
      // NOTES: adjustSizes = antiCollision & scalingRatio = coefficient

      if (W.settings.barnesHutOptimize) {
        coefficient = W.settings.scalingRatio;

        // Applying repulsion through regions
        for (n = 0; n < W.nodesLength; n += W.ppn) {

          // Computing leaf quad nodes iteration

          r = 0; // Starting with root region
          while (true) {

            if (RegionMatrix[rp(r, 'firstChild')] >= 0) {

              // The region has sub-regions

              // We run the Barnes Hut test to see if we are at the right distance
              distance = Math.sqrt(
                (Math.pow(NodeMatrix[np(n, 'x')] - RegionMatrix[rp(r, 'massCenterX')], 2)) +
                (Math.pow(NodeMatrix[np(n, 'y')] - RegionMatrix[rp(r, 'massCenterY')], 2))
              );

              if (2 * RegionMatrix[rp(r, 'size')] / distance < W.settings.barnesHutTheta) {

                // We treat the region as a single body, and we repulse

                xDist = NodeMatrix[np(n, 'x')] - RegionMatrix[rp(r, 'massCenterX')];
                yDist = NodeMatrix[np(n, 'y')] - RegionMatrix[rp(r, 'massCenterY')];

                if (W.settings.adjustSizes) {

                  //-- Linear Anti-collision Repulsion
                  if (distance > 0) {
                    factor = coefficient * NodeMatrix[np(n, 'mass')] *
                      RegionMatrix[rp(r, 'mass')] / distance / distance;

                    NodeMatrix[np(n, 'dx')] += xDist * factor;
                    NodeMatrix[np(n, 'dy')] += yDist * factor;
                  }
                  else if (distance < 0) {
                    factor = -coefficient * NodeMatrix[np(n, 'mass')] *
                      RegionMatrix[rp(r, 'mass')] / distance;

                    NodeMatrix[np(n, 'dx')] += xDist * factor;
                    NodeMatrix[np(n, 'dy')] += yDist * factor;
                  }
                }
                else {

                  //-- Linear Repulsion
                  if (distance > 0) {
                    factor = coefficient * NodeMatrix[np(n, 'mass')] *
                      RegionMatrix[rp(r, 'mass')] / distance / distance;

                    NodeMatrix[np(n, 'dx')] += xDist * factor;
                    NodeMatrix[np(n, 'dy')] += yDist * factor;
                  }
                }

                // When this is done, we iterate. We have to look at the next sibling.
                if (RegionMatrix[rp(r, 'nextSibling')] < 0)
                  break;  // No next sibling: we have finished the tree
                r = RegionMatrix[rp(r, 'nextSibling')];
                continue;

              }
              else {

                // The region is too close and we have to look at sub-regions
                r = RegionMatrix[rp(r, 'firstChild')];
                continue;
              }

            }
            else {

              // The region has no sub-region
              // If there is a node r[0] and it is not n, then repulse

              if (RegionMatrix[rp(r, 'node')] >= 0 && RegionMatrix[rp(r, 'node')] !== n) {
                xDist = NodeMatrix[np(n, 'x')] - NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'x')];
                yDist = NodeMatrix[np(n, 'y')] - NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'y')];

                distance = Math.sqrt(xDist * xDist + yDist * yDist);

                if (W.settings.adjustSizes) {

                  //-- Linear Anti-collision Repulsion
                  if (distance > 0) {
                    factor = coefficient * NodeMatrix[np(n, 'mass')] *
                      NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'mass')] / distance / distance;

                    NodeMatrix[np(n, 'dx')] += xDist * factor;
                    NodeMatrix[np(n, 'dy')] += yDist * factor;
                  }
                  else if (distance < 0) {
                    factor = -coefficient * NodeMatrix[np(n, 'mass')] *
                      NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'mass')] / distance;

                    NodeMatrix[np(n, 'dx')] += xDist * factor;
                    NodeMatrix[np(n, 'dy')] += yDist * factor;
                  }
                }
                else {

                  //-- Linear Repulsion
                  if (distance > 0) {
                    factor = coefficient * NodeMatrix[np(n, 'mass')] *
                      NodeMatrix[np(RegionMatrix[rp(r, 'node')], 'mass')] / distance / distance;

                    NodeMatrix[np(n, 'dx')] += xDist * factor;
                    NodeMatrix[np(n, 'dy')] += yDist * factor;
                  }
                }

              }

              // When this is done, we iterate. We have to look at the next sibling.
              if (RegionMatrix[rp(r, 'nextSibling')] < 0)
                break;  // No next sibling: we have finished the tree
              r = RegionMatrix[rp(r, 'nextSibling')];
              continue;
            }
          }
        }
      }
      else {
        coefficient = W.settings.scalingRatio;

        // Square iteration
        for (n1 = 0; n1 < W.nodesLength; n1 += W.ppn) {
          for (n2 = 0; n2 < n1; n2 += W.ppn) {

            // Common to both methods
            xDist = NodeMatrix[np(n1, 'x')] - NodeMatrix[np(n2, 'x')];
            yDist = NodeMatrix[np(n1, 'y')] - NodeMatrix[np(n2, 'y')];

            if (W.settings.adjustSizes) {

              //-- Anticollision Linear Repulsion
              distance = Math.sqrt(xDist * xDist + yDist * yDist) -
                NodeMatrix[np(n1, 'size')] -
                NodeMatrix[np(n2, 'size')];

              if (distance > 0) {
                factor = coefficient *
                  NodeMatrix[np(n1, 'mass')] *
                  NodeMatrix[np(n2, 'mass')] /
                  distance / distance;

                // Updating nodes' dx and dy
                NodeMatrix[np(n1, 'dx')] += xDist * factor;
                NodeMatrix[np(n1, 'dy')] += yDist * factor;

                NodeMatrix[np(n2, 'dx')] += xDist * factor;
                NodeMatrix[np(n2, 'dy')] += yDist * factor;
              }
              else if (distance < 0) {
                factor = 100 * coefficient *
                  NodeMatrix[np(n1, 'mass')] *
                  NodeMatrix[np(n2, 'mass')];

                // Updating nodes' dx and dy
                NodeMatrix[np(n1, 'dx')] += xDist * factor;
                NodeMatrix[np(n1, 'dy')] += yDist * factor;

                NodeMatrix[np(n2, 'dx')] -= xDist * factor;
                NodeMatrix[np(n2, 'dy')] -= yDist * factor;
              }
            }
            else {

              //-- Linear Repulsion
              distance = Math.sqrt(xDist * xDist + yDist * yDist);

              if (distance > 0) {
                factor = coefficient *
                  NodeMatrix[np(n1, 'mass')] *
                  NodeMatrix[np(n2, 'mass')] /
                  distance / distance;

                // Updating nodes' dx and dy
                NodeMatrix[np(n1, 'dx')] += xDist * factor;
                NodeMatrix[np(n1, 'dy')] += yDist * factor;

                NodeMatrix[np(n2, 'dx')] -= xDist * factor;
                NodeMatrix[np(n2, 'dy')] -= yDist * factor;
              }
            }
          }
        }
      }


      // 3) Gravity
      //------------
      g = W.settings.gravity / W.settings.scalingRatio;
      coefficient = W.settings.scalingRatio;
      for (n = 0; n < W.nodesLength; n += W.ppn) {
        factor = 0;

        // Common to both methods
        xDist = NodeMatrix[np(n, 'x')];
        yDist = NodeMatrix[np(n, 'y')];
        distance = Math.sqrt(
          Math.pow(xDist, 2) + Math.pow(yDist, 2)
        );

        if (W.settings.strongGravityMode) {

          //-- Strong gravity
          if (distance > 0)
            factor = coefficient * NodeMatrix[np(n, 'mass')] * g;
        }
        else {

          //-- Linear Anti-collision Repulsion n
          if (distance > 0)
            factor = coefficient * NodeMatrix[np(n, 'mass')] * g / distance;
        }

        // Updating node's dx and dy
        NodeMatrix[np(n, 'dx')] -= xDist * factor;
        NodeMatrix[np(n, 'dy')] -= yDist * factor;
      }



      // 4) Attraction
      //---------------
      coefficient = 1 *
        (W.settings.outboundAttractionDistribution ?
          outboundAttCompensation :
          1);

      // TODO: simplify distance
      // TODO: coefficient is always used as -c --> optimize?
      for (e = 0; e < W.edgesLength; e += W.ppe) {
        n1 = EdgeMatrix[ep(e, 'source')];
        n2 = EdgeMatrix[ep(e, 'target')];
        w = EdgeMatrix[ep(e, 'weight')];

        // Edge weight influence
        ewc = Math.pow(w, W.settings.edgeWeightInfluence);

        // Common measures
        xDist = NodeMatrix[np(n1, 'x')] - NodeMatrix[np(n2, 'x')];
        yDist = NodeMatrix[np(n1, 'y')] - NodeMatrix[np(n2, 'y')];

        // Applying attraction to nodes
        if (W.settings.adjustSizes) {

          distance = Math.sqrt(
            (Math.pow(xDist, 2) + Math.pow(yDist, 2)) -
            NodeMatrix[np(n1, 'size')] -
            NodeMatrix[np(n2, 'size')]
          );

          if (W.settings.linLogMode) {
            if (W.settings.outboundAttractionDistribution) {

              //-- LinLog Degree Distributed Anti-collision Attraction
              if (distance > 0) {
                factor = -coefficient * ewc * Math.log(1 + distance) /
                distance /
                NodeMatrix[np(n1, 'mass')];
              }
            }
            else {

              //-- LinLog Anti-collision Attraction
              if (distance > 0) {
                factor = -coefficient * ewc * Math.log(1 + distance) / distance;
              }
            }
          }
          else {
            if (W.settings.outboundAttractionDistribution) {

              //-- Linear Degree Distributed Anti-collision Attraction
              if (distance > 0) {
                factor = -coefficient * ewc / NodeMatrix[np(n1, 'mass')];
              }
            }
            else {

              //-- Linear Anti-collision Attraction
              if (distance > 0) {
                factor = -coefficient * ewc;
              }
            }
          }
        }
        else {

          distance = Math.sqrt(
            Math.pow(xDist, 2) + Math.pow(yDist, 2)
          );

          if (W.settings.linLogMode) {
            if (W.settings.outboundAttractionDistribution) {

              //-- LinLog Degree Distributed Attraction
              if (distance > 0) {
                factor = -coefficient * ewc * Math.log(1 + distance) /
                  distance /
                  NodeMatrix[np(n1, 'mass')];
              }
            }
            else {

              //-- LinLog Attraction
              if (distance > 0)
                factor = -coefficient * ewc * Math.log(1 + distance) / distance;
            }
          }
          else {
            if (W.settings.outboundAttractionDistribution) {

              //-- Linear Attraction Mass Distributed
              // NOTE: Distance is set to 1 to override next condition
              distance = 1;
              factor = -coefficient * ewc / NodeMatrix[np(n1, 'mass')];
            }
            else {

              //-- Linear Attraction
              // NOTE: Distance is set to 1 to override next condition
              distance = 1;
              factor = -coefficient * ewc;
            }
          }
        }

        // Updating nodes' dx and dy
        // TODO: if condition or factor = 1?
        if (distance > 0) {

          // Updating nodes' dx and dy
          NodeMatrix[np(n1, 'dx')] += xDist * factor;
          NodeMatrix[np(n1, 'dy')] += yDist * factor;

          NodeMatrix[np(n2, 'dx')] -= xDist * factor;
          NodeMatrix[np(n2, 'dy')] -= yDist * factor;
        }
      }


      // 5) Apply Forces
      //-----------------
      var force,
          swinging,
          traction,
          nodespeed;

      // MATH: sqrt and square distances
      if (W.settings.adjustSizes) {

        for (n = 0; n < W.nodesLength; n += W.ppn) {
          if (!NodeMatrix[np(n, 'fixed')]) {
            force = Math.sqrt(
              Math.pow(NodeMatrix[np(n, 'dx')], 2) +
              Math.pow(NodeMatrix[np(n, 'dy')], 2)
            );

            if (force > W.maxForce) {
              NodeMatrix[np(n, 'dx')] =
                NodeMatrix[np(n, 'dx')] * W.maxForce / force;
              NodeMatrix[np(n, 'dy')] =
                NodeMatrix[np(n, 'dy')] * W.maxForce / force;
            }

            swinging = NodeMatrix[np(n, 'mass')] *
              Math.sqrt(
                (NodeMatrix[np(n, 'old_dx')] - NodeMatrix[np(n, 'dx')]) *
                (NodeMatrix[np(n, 'old_dx')] - NodeMatrix[np(n, 'dx')]) +
                (NodeMatrix[np(n, 'old_dy')] - NodeMatrix[np(n, 'dy')]) *
                (NodeMatrix[np(n, 'old_dy')] - NodeMatrix[np(n, 'dy')])
              );

            traction = Math.sqrt(
              (NodeMatrix[np(n, 'old_dx')] + NodeMatrix[np(n, 'dx')]) *
              (NodeMatrix[np(n, 'old_dx')] + NodeMatrix[np(n, 'dx')]) +
              (NodeMatrix[np(n, 'old_dy')] + NodeMatrix[np(n, 'dy')]) *
              (NodeMatrix[np(n, 'old_dy')] + NodeMatrix[np(n, 'dy')])
            ) / 2;

            nodespeed =
              0.1 * Math.log(1 + traction) / (1 + Math.sqrt(swinging));

            // Updating node's positon
            NodeMatrix[np(n, 'x')] =
              NodeMatrix[np(n, 'x')] + NodeMatrix[np(n, 'dx')] *
              (nodespeed / W.settings.slowDown);
            NodeMatrix[np(n, 'y')] =
              NodeMatrix[np(n, 'y')] + NodeMatrix[np(n, 'dy')] *
              (nodespeed / W.settings.slowDown);
          }
        }
      }
      else {

        for (n = 0; n < W.nodesLength; n += W.ppn) {
          if (!NodeMatrix[np(n, 'fixed')]) {

            swinging = NodeMatrix[np(n, 'mass')] *
              Math.sqrt(
                (NodeMatrix[np(n, 'old_dx')] - NodeMatrix[np(n, 'dx')]) *
                (NodeMatrix[np(n, 'old_dx')] - NodeMatrix[np(n, 'dx')]) +
                (NodeMatrix[np(n, 'old_dy')] - NodeMatrix[np(n, 'dy')]) *
                (NodeMatrix[np(n, 'old_dy')] - NodeMatrix[np(n, 'dy')])
              );

            traction = Math.sqrt(
              (NodeMatrix[np(n, 'old_dx')] + NodeMatrix[np(n, 'dx')]) *
              (NodeMatrix[np(n, 'old_dx')] + NodeMatrix[np(n, 'dx')]) +
              (NodeMatrix[np(n, 'old_dy')] + NodeMatrix[np(n, 'dy')]) *
              (NodeMatrix[np(n, 'old_dy')] + NodeMatrix[np(n, 'dy')])
            ) / 2;

            nodespeed = NodeMatrix[np(n, 'convergence')] *
              Math.log(1 + traction) / (1 + Math.sqrt(swinging));

            // Updating node convergence
            NodeMatrix[np(n, 'convergence')] =
              Math.min(1, Math.sqrt(
                nodespeed *
                (Math.pow(NodeMatrix[np(n, 'dx')], 2) +
                 Math.pow(NodeMatrix[np(n, 'dy')], 2)) /
                (1 + Math.sqrt(swinging))
              ));

            // Updating node's positon
            NodeMatrix[np(n, 'x')] =
              NodeMatrix[np(n, 'x')] + NodeMatrix[np(n, 'dx')] *
              (nodespeed / W.settings.slowDown);
            NodeMatrix[np(n, 'y')] =
              NodeMatrix[np(n, 'y')] + NodeMatrix[np(n, 'dy')] *
              (nodespeed / W.settings.slowDown);
          }
        }
      }

      // Counting one more iteration
      W.iterations++;
    }

    /**
     * Message reception & sending
     */

    // Sending data back to the supervisor
    var sendNewCoords;

    if (typeof window !== 'undefined' && window.document) {

      // From same document as sigma
      sendNewCoords = function() {
        var e;

        if (document.createEvent) {
          e = document.createEvent('Event');
          e.initEvent('newCoords', true, false);
        }
        else {
          e = document.createEventObject();
          e.eventType = 'newCoords';
        }

        e.eventName = 'newCoords';
        e.data = {
          nodes: NodeMatrix.buffer
        };
        requestAnimationFrame(function() {
          document.dispatchEvent(e);
        });
      };
    }
    else {

      // From a WebWorker
      sendNewCoords = function() {
        self.postMessage(
          {nodes: NodeMatrix.buffer},
          [NodeMatrix.buffer]
        );
      };
    }

    // Algorithm run
    function run(n) {
      for (var i = 0; i < n; i++)
        pass();
      sendNewCoords();
    }

    // On supervisor message
    var listener = function(e) {
      switch (e.data.action) {
        case 'start':
          init(
            new Float32Array(e.data.nodes),
            new Float32Array(e.data.edges),
            e.data.config
          );

          // First iteration(s)
          run(W.settings.startingIterations);
          break;

        case 'loop':
          NodeMatrix = new Float32Array(e.data.nodes);
          run(W.settings.iterationsPerRender);
          break;

        case 'config':

          // Merging new settings
          configure(e.data.config);
          break;

        case 'kill':

          // Deleting context for garbage collection
          __emptyObject(W);
          NodeMatrix = null;
          EdgeMatrix = null;
          RegionMatrix = null;
          self.removeEventListener('message', listener);
          break;

        default:
      }
    };

    // Adding event listener
    self.addEventListener('message', listener);
  };


  /**
   * Exporting
   * ----------
   *
   * Crush the worker function and make it accessible by sigma's instances so
   * the supervisor can call it.
   */
  function crush(fnString) {
    var pattern,
        i,
        l;

    var np = [
      'x',
      'y',
      'dx',
      'dy',
      'old_dx',
      'old_dy',
      'mass',
      'convergence',
      'size',
      'fixed'
    ];

    var ep = [
      'source',
      'target',
      'weight'
    ];

    var rp = [
      'node',
      'centerX',
      'centerY',
      'size',
      'nextSibling',
      'firstChild',
      'mass',
      'massCenterX',
      'massCenterY'
    ];

    // rp
    // NOTE: Must go first
    for (i = 0, l = rp.length; i < l; i++) {
      pattern = new RegExp('rp\\(([^,]*), \'' + rp[i] + '\'\\)', 'g');
      fnString = fnString.replace(
        pattern,
        (i === 0) ? '$1' : '$1 + ' + i
      );
    }

    // np
    for (i = 0, l = np.length; i < l; i++) {
      pattern = new RegExp('np\\(([^,]*), \'' + np[i] + '\'\\)', 'g');
      fnString = fnString.replace(
        pattern,
        (i === 0) ? '$1' : '$1 + ' + i
      );
    }

    // ep
    for (i = 0, l = ep.length; i < l; i++) {
      pattern = new RegExp('ep\\(([^,]*), \'' + ep[i] + '\'\\)', 'g');
      fnString = fnString.replace(
        pattern,
        (i === 0) ? '$1' : '$1 + ' + i
      );
    }

    return fnString;
  }

  // Exporting
  function getWorkerFn() {
    var fnString = crush ? crush(Worker.toString()) : Worker.toString();
    return ';(' + fnString + ').call(this);';
  }

  if (inWebWorker) {

    // We are in a webworker, so we launch the Worker function
    eval(getWorkerFn());
  }
  else {

    // We are requesting the worker from sigma, we retrieve it therefore
    if (typeof sigma === 'undefined')
      throw 'sigma is not declared';

    sigma.prototype.getForceAtlas2Worker = getWorkerFn;
  }
}).call(this);
