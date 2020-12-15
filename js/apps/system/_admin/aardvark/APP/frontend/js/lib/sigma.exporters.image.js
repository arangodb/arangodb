;(function(undefined) {
  'use strict';

  if (typeof sigma === 'undefined')
    throw 'sigma is not declared';

  // Initialize package:
  sigma.utils.pkg('sigma.plugins');

  /**
   * Sigma Image Utility
   * =============================
   *
   * @author: Martin de la Taille (martindelataille)
   * @thanks: Guillaume Plique (Yomguithereal)
   * @version: 0.1
   */

  var _contexts,
      _types,
      _canvas,
      _canvasContext;

  _contexts = ['scene', 'edges', 'nodes', 'labels'];
  _types = {
    png: 'image/png',
    jpg: 'image/jpeg',
    gif: 'image/gif',
    tiff: 'image/tiff'
  };

  // UTILITIES FUNCTIONS:
  // ******************
  function dataURLToBlob(dataURL) {
    var BASE64_MARKER = ';base64,';
    if (dataURL.indexOf(BASE64_MARKER) == -1) {
      var parts = dataURL.split(',');
      var contentType = parts[0].split(':')[1];
      var raw = decodeURIComponent(parts[1]);

      return new Blob([raw], {type: contentType});
    }

    var parts = dataURL.split(BASE64_MARKER);
    var contentType = parts[0].split(':')[1];
    var raw = window.atob(parts[1]);
    var rawLength = raw.length;

    var uInt8Array = new Uint8Array(rawLength);

    for (var i = 0; i < rawLength; ++i) {
      uInt8Array[i] = raw.charCodeAt(i);
    }

    return new Blob([uInt8Array], {type: contentType});
  }

  function download(dataUrl, extension, filename) {
    filename = filename || 'graph.' + extension;

    if (navigator.msSaveOrOpenBlob) { // IE10+
      navigator.msSaveOrOpenBlob(dataURLToBlob(dataUrl), filename);
    }
    else if (navigator.msSaveBlob) { // IE11+
      navigator.msSaveBlob(dataURLToBlob(dataUrl), filename);
    }
    else {
      var anchor = document.createElement('a');
      anchor.setAttribute('href', dataUrl);
      anchor.setAttribute('download', filename);

      // Firefox requires the link to be added to the DOM before it can be clicked.
      document.body.appendChild(anchor);
      anchor.click();
      document.body.removeChild(anchor);
    }
  }

  function calculateAspectRatioFit(srcWidth, srcHeight, maxSize) {
    var ratio = Math.min(maxSize / srcWidth, maxSize / srcHeight);
    return { width: srcWidth * ratio, height: srcHeight * ratio };
  }

  function calculateZoomedBoundaries(s, r, params) {
    var bounds;

    bounds = sigma.utils.getBoundaries(
      s.graph,
      r.camera.readPrefix
    );

    bounds.minX /= params.zoomRatio;
    bounds.minY /= params.zoomRatio;
    bounds.maxX /= params.zoomRatio;
    bounds.maxY /= params.zoomRatio;

    return bounds;
  }

  function calculateRatio(s, r, params) {
    var boundaries,
        margin = params.margin || 0,
        ratio = {
          width:  r.width,
          height: r.height,
        };

    if (!params.clips && !params.size) {
      boundaries = calculateZoomedBoundaries(s, r, params);

      ratio = {
        width:  boundaries.maxX - boundaries.minX + boundaries.sizeMax * 2,
        height: boundaries.maxY - boundaries.minY + boundaries.sizeMax * 2
      };
    }
    else if (params.size && params.size >= 1) {
      ratio = calculateAspectRatioFit(r.width, r.height, params.size);
    }

    ratio.width += margin;
    ratio.height += margin;

    return ratio;
  }

  /**
  * This function generate a new canvas to download image
  *
  * Recognized parameters:
  * **********************
  * Here is the exhaustive list of every accepted parameters in the settings
  * object:
  * @param {s}  sigma instance
  * @param {params}  Options
  */
 function Image(s, r, params) {
    params = params || {};

    if (params.format && !(params.format in _types))
      throw Error('sigma.renderers.image: unsupported format "' + params.format + '".');

    var ratio = calculateRatio(s, r, params);
    var batchEdgesDrawing = s.settings('batchEdgesDrawing');
    if (batchEdgesDrawing) {
      s.settings('batchEdgesDrawing', false); // it may crash if true
    }

    if(!params.clip)
      this.clone(s, params, ratio);

    var merged = this.draw(r, params, ratio);

    s.settings('batchEdgesDrawing', batchEdgesDrawing); // restore setting

    var dataUrl = merged.toDataURL(_types[params.format || 'png']);

    if(params.download)
      download(
        dataUrl,
        params.format || 'png',
        params.filename
      );

    return dataUrl;
  }

  /**
  * @param {s}  sigma instance
  * @param {params}  Options
  */
  Image.prototype.clone = function(s, params, ratio) {
    params.tmpContainer = params.tmpContainer || 'image-container';

    var pixelRatio = sigma.utils.getPixelRatio();
    var webglOversamplingRatio = s.settings('webglOversamplingRatio');
    var el = document.getElementById(params.tmpContainer);

    if (!el) {
      el =  document.createElement("div");
      el.id = params.tmpContainer;
      document.body.appendChild(el);
    }
    el.setAttribute("style",
      'width:' + Math.round(ratio.width / pixelRatio) + 'px;' +
      'height:' + Math.round(ratio.height / pixelRatio) + 'px;');

    var renderer = s.addRenderer({
      container: document.getElementById(params.tmpContainer),
      type: 'canvas',
      settings: {
        batchEdgesDrawing: true,
        drawLabels: !!params.labels
      }
    });

    renderer.camera.ratio = (params.zoomRatio > 0) ? params.zoomRatio : 1;
    if (!params.size) {
      renderer.camera.ratio *= pixelRatio;
    }

    var webgl = renderer instanceof sigma.renderers.webgl,
        sized = false,
        doneContexts = [];

    _canvas = document.createElement('canvas');
    _canvasContext = _canvas.getContext('2d');

    s.refresh();

    _contexts.forEach(function(name) {
      if (!renderer.contexts[name])
        return;

      if (params.labels === false && name === 'labels')
        return;

      var canvas = renderer.domElements[name] || renderer.domElements.scene,
        context = renderer.contexts[name];

      if(!sized) {
        _canvas.width = ratio.width;
        _canvas.height = ratio.height;

        if (webgl && context instanceof WebGLRenderingContext) {
          _canvas.width  /= webglOversamplingRatio;
          _canvas.height /= webglOversamplingRatio;
        }

        sized = true;
      }

      if (context instanceof WebGLRenderingContext)
        _canvasContext.drawImage(canvas, 0, 0,
          canvas.width / webglOversamplingRatio,
          canvas.height / webglOversamplingRatio
      );
      else
        _canvasContext.drawImage(canvas, 0, 0);

      if (~doneContexts.indexOf(context))
        return;

      doneContexts.push(context);
    });

    // Cleaning
    doneContexts = [];
    s.killRenderer(renderer);
    el.parentNode.removeChild(el);
  }

  /**
  * @param {renderer}  related renderer instance
  * @param {params}  Options
  */
  Image.prototype.draw = function(r, params, ratio) {
    var webgl = r instanceof sigma.renderers.webgl,
        sized = false,
        doneContexts = [];

    var merged = document.createElement('canvas'),
        mergedContext= merged.getContext('2d');

    _contexts.forEach(function(name) {
      if (!r.contexts[name])
        return;

      if (params.labels === false && name === 'labels')
        return;

      var canvas = r.domElements[name] || r.domElements.scene,
        context = r.contexts[name];

      if (~doneContexts.indexOf(context))
        return;

      if (!sized) {

        var width, height;

        if(!params.clip) {
          width = _canvas.width;
          height = _canvas.height;
        }
        else {
          var size = (!params.size || params.size < 1) ? window.innerWidth : params.size;
          width = canvas.width;
          height = canvas.height;
          ratio = calculateAspectRatioFit(width, height, size);
        }

        merged.width = ratio.width;
        merged.height = ratio.height;

        if (!webgl && !context instanceof WebGLRenderingContext) {
          var webglOversamplingRatio = s.settings('webglOversamplingRatio');
          merged.width *= webglOversamplingRatio;
          merged.height *= webglOversamplingRatio;
        }

        sized = true;

        // background color
        if (params.background) {
          mergedContext.rect(0, 0, merged.width, merged.height);
          mergedContext.fillStyle = params.background;
          mergedContext.fill();
        }
      }

      mergedContext.drawImage(
        (params.clip) ? canvas : _canvas,
        0,
        0,
        merged.width,
        merged.height
      );

      doneContexts.push(context);
    });

    // Cleaning
    doneContexts = [];

    return merged;
  }

  /**
   * Interface
   * ------------------
   */
  var _instance = null;

  /**
   * @param {sigma}  s       The related sigma instance.
   * @param {renderer}  r    The related renderer instance.
   * @param {object} options An object with options.
   */
  sigma.plugins.image = function(s, r, options) {
    sigma.plugins.killImage();
    // Create object if undefined
    if (!_instance) {
      _instance = new Image(s, r, options);
    }
    return _instance;
  };

  /**
   *  This function kills the image instance.
   */
  sigma.plugins.killImage = function() {
    if (_instance instanceof Image) {
      _instance = null;
      _canvas = null;
      _canvasContext = null;
    }
  };

}).call(this);
