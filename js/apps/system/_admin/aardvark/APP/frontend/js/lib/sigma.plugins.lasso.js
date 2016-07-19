/**
 * Sigma Lasso
 * =============================
 *
 * @author Florent Schildknecht <florent.schildknecht@gmail.com> (Florent Schildknecht)
 * @version 0.0.2
 */
;(function (undefined) {
  'use strict';

  if (typeof sigma === 'undefined')
    throw 'sigma is not declared';

  // Initialize package:
  sigma.utils.pkg('sigma.plugins');

   var _body = undefined,
       _instances = {};

  /**
   * Lasso Object
   * ------------------
   * @param  {sigma}                                  sigmaInstance The related sigma instance.
   * @param  {renderer} renderer                      The sigma instance renderer.
   * @param  {sigma.classes.configurable} settings    A settings class
   */
  function Lasso (sigmaInstance, renderer, settings) {
    // Lasso is also an event dispatcher
    sigma.classes.dispatcher.extend(this);

    // A quick hardcoded rule to prevent people from using this plugin with the
    // WebGL renderer (which is impossible at the moment):
    if (
      sigma.renderers.webgl &&
      renderer instanceof sigma.renderers.webgl
    )
      throw new Error(
        'The sigma.plugins.lasso is not compatible with the WebGL renderer'
      );

    this.sigmaInstance = sigmaInstance;
    this.renderer = renderer;
    this.drawingCanvas = undefined;
    this.drawingContext = undefined;
    this.drewPoints = [];
    this.selectedNodes = [];
    this.isActive = false;
    this.isDrawing = false;

    _body = document.body;

    // Extends default settings
    this.settings = new sigma.classes.configurable({
      'strokeStyle': 'black',
      'lineWidth': 2,
      'fillWhileDrawing': false,
      'fillStyle': 'rgba(200, 200, 200, 0.25)',
      'cursor': 'crosshair'
     }, settings || {});
  };

  /**
   * This method is used to destroy the lasso.
   *
   * > var lasso = new sigma.plugins.lasso(sigmaInstance);
   * > lasso.clear();
   *
   * @return {sigma.plugins.lasso} Returns the instance.
   */
  Lasso.prototype.clear = function () {
    this.deactivate();

    this.sigmaInstance = undefined;
    this.renderer = undefined;

    return this;
  };

  // Lasso.prototype.getSigmaInstance = function () {
  //   return this.sigmaInstance;
  // }

  /**
   * This method is used to activate the lasso mode.
   *
   * > var lasso = new sigma.plugins.lasso(sigmaInstance);
   * > lasso.activate();
   *
   * @return {sigma.plugins.lasso} Returns the instance.
   */
  Lasso.prototype.activate = function () {
    if (this.sigmaInstance && !this.isActive) {
      this.isActive = true;

      // Add a new background layout canvas to draw the path on
      if (!this.renderer.domElements['lasso']) {
        this.renderer.initDOM('canvas', 'lasso');
        this.drawingCanvas = this.renderer.domElements['lasso'];

        this.drawingCanvas.width = this.renderer.container.offsetWidth;
        this.drawingCanvas.height = this.renderer.container.offsetHeight;
        this.renderer.container.appendChild(this.drawingCanvas);
        this.drawingContext = this.drawingCanvas.getContext('2d');
        this.drawingCanvas.style.cursor = this.settings('cursor');
      }

      _bindAll.apply(this);
    }

    return this;
  };

  /**
   * This method is used to deactivate the lasso mode.
   *
   * > var lasso = new sigma.plugins.lasso(sigmaInstance);
   * > lasso.deactivate();
   *
   * @return {sigma.plugins.lasso} Returns the instance.
   */
  Lasso.prototype.deactivate = function () {
    if (this.sigmaInstance && this.isActive) {
      this.isActive = false;
      this.isDrawing = false;

      _unbindAll.apply(this);

      if (this.renderer.domElements['lasso']) {
        this.renderer.container.removeChild(this.renderer.domElements['lasso']);
        delete this.renderer.domElements['lasso'];
        this.drawingCanvas.style.cursor = '';
        this.drawingCanvas = undefined;
        this.drawingContext = undefined;
        this.drewPoints = [];
      }
    }

    return this;
  };

  /**
   * This method is used to bind all events of the lasso mode.
   *
   * > var lasso = new sigma.plugins.lasso(sigmaInstance);
   * > lasso.activate();
   *
   * @return {sigma.plugins.lasso} Returns the instance.
   */
  var _bindAll = function () {
    // Mouse events
    this.drawingCanvas.addEventListener('mousedown', onDrawingStart.bind(this));
    _body.addEventListener('mousemove', onDrawing.bind(this));
    _body.addEventListener('mouseup', onDrawingEnd.bind(this));
    // Touch events
    this.drawingCanvas.addEventListener('touchstart', onDrawingStart.bind(this));
    _body.addEventListener('touchmove', onDrawing.bind(this));
    _body.addEventListener('touchcancel', onDrawingEnd.bind(this));
    _body.addEventListener('touchleave', onDrawingEnd.bind(this));
    _body.addEventListener('touchend', onDrawingEnd.bind(this));
  };

  /**
   * This method is used to unbind all events of the lasso mode.
   *
   * > var lasso = new sigma.plugins.lasso(sigmaInstance);
   * > lasso.activate();
   *
   * @return {sigma.plugins.lasso} Returns the instance.
   */
  var _unbindAll = function () {
    // Mouse events
    this.drawingCanvas.removeEventListener('mousedown', onDrawingStart.bind(this));
    _body.removeEventListener('mousemove', onDrawing.bind(this));
    _body.removeEventListener('mouseup', onDrawingEnd.bind(this));
    // Touch events
    this.drawingCanvas.removeEventListener('touchstart', onDrawingStart.bind(this));
    this.drawingCanvas.removeEventListener('touchmove', onDrawing.bind(this));
    _body.removeEventListener('touchcancel', onDrawingEnd.bind(this));
    _body.removeEventListener('touchleave', onDrawingEnd.bind(this));
    _body.removeEventListener('touchend', onDrawingEnd.bind(this));
  };

  /**
   * This method is used to retrieve the previously selected nodes
   *
   * > var lasso = new sigma.plugins.lasso(sigmaInstance);
   * > lasso.getSelectedNodes();
   *
   * @return {array} Returns an array of nodes.
   */
  Lasso.prototype.getSelectedNodes = function () {
    return this.selectedNodes;
  };

  function onDrawingStart (event) {
    var drawingRectangle = this.drawingCanvas.getBoundingClientRect();

    if (this.isActive) {
      this.isDrawing = true;
      this.drewPoints = [];
      this.selectedNodes = [];

      this.sigmaInstance.refresh();

      this.drewPoints.push({
        x: event.clientX - drawingRectangle.left,
        y: event.clientY - drawingRectangle.top
      });

      this.drawingCanvas.style.cursor = this.settings('cursor');

      event.stopPropagation();
    }
  }

  function onDrawing (event) {
    if (this.isActive && this.isDrawing) {
      var x = 0,
          y = 0,
          drawingRectangle = this.drawingCanvas.getBoundingClientRect();
      switch (event.type) {
        case 'touchmove':
          x = event.touches[0].clientX;
          y = event.touches[0].clientY;
          break;
        default:
          x = event.clientX;
          y = event.clientY;
          break;
      }
      this.drewPoints.push({
        x: x - drawingRectangle.left,
        y: y - drawingRectangle.top
      });

      // Drawing styles
      this.drawingContext.lineWidth = this.settings('lineWidth');
      this.drawingContext.strokeStyle = this.settings('strokeStyle');
      this.drawingContext.fillStyle = this.settings('fillStyle');
      this.drawingContext.lineJoin = 'round';
      this.drawingContext.lineCap = 'round';

      // Clear the canvas
      this.drawingContext.clearRect(0, 0, this.drawingContext.canvas.width, this.drawingContext.canvas.height);

      // Redraw the complete path for a smoother effect
      // Even smoother with quadratic curves
      var sourcePoint = this.drewPoints[0],
          destinationPoint = this.drewPoints[1],
          pointsLength = this.drewPoints.length,
          getMiddlePointCoordinates = function (firstPoint, secondPoint) {
            return {
              x: firstPoint.x + (secondPoint.x - firstPoint.x) / 2,
              y: firstPoint.y + (secondPoint.y - firstPoint.y) / 2
            };
          };

      this.drawingContext.beginPath();
      this.drawingContext.moveTo(sourcePoint.x, sourcePoint.y);

      for (var i = 1; i < pointsLength; i++) {
        var middlePoint = getMiddlePointCoordinates(sourcePoint, destinationPoint);
        // this.drawingContext.lineTo(this.drewPoints[i].x, this.drewPoints[i].y);
        this.drawingContext.quadraticCurveTo(sourcePoint.x, sourcePoint.y, middlePoint.x, middlePoint.y);
        sourcePoint = this.drewPoints[i];
        destinationPoint = this.drewPoints[i+1];
      }

      this.drawingContext.lineTo(sourcePoint.x, sourcePoint.y);
      this.drawingContext.stroke();

      if (this.settings('fillWhileDrawing')) {
        this.drawingContext.fill();
      }

      event.stopPropagation();
    }
  }

  function onDrawingEnd (event) {
    if (this.isActive && this.isDrawing) {
      this.isDrawing = false;

      // Select the nodes inside the path
      var nodes = this.renderer.nodesOnScreen,
        nodesLength = nodes.length,
        i = 0,
        prefix = this.renderer.options.prefix || '';

      // Loop on all nodes and check if they are in the path
      while (nodesLength--) {
        var node = nodes[nodesLength],
            x = node[prefix + 'x'],
            y = node[prefix + 'y'];

        if (this.drawingContext.isPointInPath(x, y) && !node.hidden) {
          this.selectedNodes.push(node);
        }
      }

      // Dispatch event with selected nodes
      this.dispatchEvent('selectedNodes', this.selectedNodes);

      // Clear the drawing canvas
      this.drawingContext.clearRect(0, 0, this.drawingCanvas.width, this.drawingCanvas.height);

      this.drawingCanvas.style.cursor = this.settings('cursor');

      event.stopPropagation();
    }
  }

  /**
   * @param  {sigma}                                  sigmaInstance The related sigma instance.
   * @param  {renderer} renderer                      The sigma instance renderer.
   * @param  {sigma.classes.configurable} settings    A settings class
   *
   * @return {sigma.plugins.lasso} Returns the instance
   */
  sigma.plugins.lasso = function (sigmaInstance, renderer, settings) {
    // Create lasso if undefined
    if (!_instances[sigmaInstance.id]) {
      _instances[sigmaInstance.id] = new Lasso(sigmaInstance, renderer, settings);
    }

    // Listen for sigmaInstance kill event, and remove the lasso isntance
    sigmaInstance.bind('kill', function () {
      if (_instances[sigmaInstance.id] instanceof Lasso) {
        _instances[sigmaInstance.id].clear();
        delete _instances[sigmaInstance.id];
      }
    });

    return _instances[sigmaInstance.id];
  };

}).call(this);
