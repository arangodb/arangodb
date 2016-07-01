;(function(undefined) {
  'use strict';

  if (typeof sigma === 'undefined')
    throw 'sigma is not declared';

  // Initialize package:
  sigma.utils.pkg('sigma.plugins.fullScreen');

  /**
   * Sigma Fullscreen
   * =============================
   *
   * @author Martin de la Taille <https://github.com/Martindelataille>
   * @author Sébastien Heymann <https://github.com/sheymann>
   * @version 0.2
   */

  var _container = null,
      _eventListenerElement = null;

  function toggleFullScreen() {
    if (!document.fullscreenElement &&    // alternative standard method
        !document.mozFullScreenElement && !document.webkitFullscreenElement && !document.msFullscreenElement ) {  // current working methods
      if (_container.requestFullscreen) {
        _container.requestFullscreen();
      } else if (_container.msRequestFullscreen) {
        _container.msRequestFullscreen();
      } else if (_container.mozRequestFullScreen) {
        _container.mozRequestFullScreen();
      } else if (_container.webkitRequestFullscreen) {
        _container.webkitRequestFullscreen(Element.ALLOW_KEYBOARD_INPUT);
      }
    } else {
      if (document.exitFullscreen) {
        document.exitFullscreen();
      } else if (document.msExitFullscreen) {
        document.msExitFullscreen();
      } else if (document.mozCancelFullScreen) {
        document.mozCancelFullScreen();
      } else if (document.webkitExitFullscreen) {
        document.webkitExitFullscreen();
      }
    }
  }

  /**
   * This plugin enables the activation of full screen mode by clicking on btn.
   * If btn does not exist, this plugin will leave the full screen mode.
   *
   * @param  {?object} options The configuration. Can contain:
   *         {?string|DOMElement} options.container A container object or id,
   *                              otherwise sigma container is used.
   *         {?string} options.btnId A btn id.
   */
  function fullScreen(options) {
    var o = options || {};

    if (o.container) {
      if (typeof o.container === 'object') {
        _container = o.container;
      }
      else {
        _container = document.getElementById(o.container)
      }
    }
    else {
      _container = this.container;
    }

    _eventListenerElement = null;

    // Get the btn element reference from the DOM
    if(o.btnId) {
      _eventListenerElement = document.getElementById(o.btnId);
      _eventListenerElement.removeEventListener("click", toggleFullScreen);
      _eventListenerElement.addEventListener("click", toggleFullScreen);
    }
    else {
      toggleFullScreen();
    }
  };

  /**
   *  This function kills the fullScreen instance.
   */
  function killFullScreen() {
    toggleFullScreen();
    _container = null;

    if (_eventListenerElement)
      _eventListenerElement.removeEventListener("click", toggleFullScreen);
  };

  sigma.plugins.fullScreen = fullScreen;
}).call(this);
