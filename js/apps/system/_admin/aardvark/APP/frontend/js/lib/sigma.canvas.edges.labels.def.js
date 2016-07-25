;(function(undefined) {
  'use strict';

  if (typeof sigma === 'undefined')
    throw 'sigma is not declared';

  // Initialize packages:
  sigma.utils.pkg('sigma.canvas.edges.labels');

  /**
   * This label renderer will just display the label on the line of the edge.
   * The label is rendered at half distance of the edge extremities, and is
   * always oriented from left to right on the top side of the line.
   *
   * @param  {object}                   edge         The edge object.
   * @param  {object}                   source node  The edge source node.
   * @param  {object}                   target node  The edge target node.
   * @param  {CanvasRenderingContext2D} context      The canvas context.
   * @param  {configurable}             settings     The settings function.
   */
  sigma.canvas.edges.labels.def =
    function(edge, source, target, context, settings) {
    if (typeof edge.label !== 'string' || source == target)
      return;

    var prefix = settings('prefix') || '',
        size = edge[prefix + 'size'] || 1;

    if (size < settings('edgeLabelThreshold'))
      return;

    if (0 === settings('edgeLabelSizePowRatio'))
      throw '"edgeLabelSizePowRatio" must not be 0.';

    var fontSize,
        x = (source[prefix + 'x'] + target[prefix + 'x']) / 2,
        y = (source[prefix + 'y'] + target[prefix + 'y']) / 2,
        dX = target[prefix + 'x'] - source[prefix + 'x'],
        dY = target[prefix + 'y'] - source[prefix + 'y'],
        sign = (source[prefix + 'x'] < target[prefix + 'x']) ? 1 : -1,
        angle = Math.atan2(dY * sign, dX * sign);

    // The font size is sublineraly proportional to the edge size, in order to
    // avoid very large labels on screen.
    // This is achieved by f(x) = x * x^(-1/ a), where 'x' is the size and 'a'
    // is the edgeLabelSizePowRatio. Notice that f(1) = 1.
    // The final form is:
    // f'(x) = b * x * x^(-1 / a), thus f'(1) = b. Application:
    // fontSize = defaultEdgeLabelSize if edgeLabelSizePowRatio = 1
    fontSize = (settings('edgeLabelSize') === 'fixed') ?
      settings('defaultEdgeLabelSize') :
      settings('defaultEdgeLabelSize') *
      size *
      Math.pow(size, -1 / settings('edgeLabelSizePowRatio'));

    context.save();

    if (edge.active) {
      context.font = [
        settings('activeFontStyle'),
        fontSize + 'px',
        settings('activeFont') || settings('font')
      ].join(' ');

      context.fillStyle =
        settings('edgeActiveColor') === 'edge' ?
        (edge.active_color || settings('defaultEdgeActiveColor')) :
        settings('defaultEdgeLabelActiveColor');
    }
    else {
      context.font = [
        settings('fontStyle'),
        fontSize + 'px',
        settings('font')
      ].join(' ');

      context.fillStyle =
        (settings('edgeLabelColor') === 'edge') ?
        (edge.color || settings('defaultEdgeColor')) :
        settings('defaultEdgeLabelColor');
    }

    context.textAlign = 'center';
    context.textBaseline = 'alphabetic';

    context.translate(x, y);
    context.rotate(angle);
    context.fillText(
      edge.label,
      0,
      (-size / 2) - 3
    );

    context.restore();
  };
}).call(this);
