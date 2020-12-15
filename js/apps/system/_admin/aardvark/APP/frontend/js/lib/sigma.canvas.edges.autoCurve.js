;(function() {
  'use strict';

  sigma.utils.pkg('sigma.canvas.edges');


  var calc = function(ratio, n, i, sortByDirection) {
    if (sortByDirection) {
      // sort edges by direction:
      var d = (ratio * n) / i;
      return { y: d ? d : Number.POSITIVE_INFINITY };
    }

    var step = ratio / (n / 2);
    var d = ratio - step * i;
    return { y: d ? 1 / d : n };
  };

  /**
   * Curves multiple edges between two nodes (i.e. "parallel edges").
   * This method is not a renderer. It should be called after modification
   * of the graph structure.
   * Time complexity: 2 * O(|E|)
   *
   * Settings: autoCurveRatio, autoCurveSortByDirection
   *
   * @param {object} s The sigma instance
   */
  sigma.canvas.edges.autoCurve = function(s) {
    var
      key,
      ratio = s.settings('autoCurveRatio'),
      sortByDirection = s.settings('autoCurveSortByDirection'),
      defaultEdgeType = s.settings('defaultEdgeType'),
      edges = s.graph.edges();

    var count = {
      key: function(o) {
        var key = o.source + ',' + o.target;
        if (this[key]) {
          return key;
        }
        if (!sortByDirection) {
          key = o.target + ',' + o.source;
          if (this[key]) {
            return key;
          }
        }

        if (sortByDirection && this[o.target + ',' + o.source]) {
          // count a parallel edge if an opposite edge exists
          this[key] = { i: 1, n: 1 };
        }
        else {
          this[key] = { i: 0, n: 0 };
        }
        return key;
      },
      inc: function(o) {
        // number of edges parallel to this one (included)
        this[this.key(o)].n++;
      }
    };

    edges.forEach(function(edge) {
      count.inc(edge);
    });

    edges.forEach(function(edge) {
      key = count.key(edge);

      // if the edge has parallel edges:
      if (count[key].n > 1 || count[key].i > 0) {
        if (!edge.cc) {
          // update edge type:
          if (edge.type === 'arrow' || edge.type === 'tapered' ||
            defaultEdgeType === 'arrow' || defaultEdgeType === 'tapered') {

            if (!edge.cc_prev_type) {
              edge.cc_prev_type = edge.type;
            }
            edge.type = 'curvedArrow';
          }
          else {
            if (!edge.cc_prev_type) {
              edge.cc_prev_type = edge.type;
            }
            edge.type = 'curve';
          }
        }

        // curvature coefficients
        edge.cc = calc(ratio, count[key].n, count[key].i++, sortByDirection);
      }
      else if (edge.cc) {
        // the edge is no longer a parallel edge
        edge.type = edge.cc_prev_type;
        edge.cc_prev_type = undefined;
        edge.cc = undefined;
      }
    });
  };

})();
