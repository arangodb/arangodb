/*jshint unused: false */
/*global window, $, Backbone, document */

(function() {
  "use strict";
  // We have to start the app only in production mode, not in test mode
  if (!window.hasOwnProperty("TEST_BUILD")) {
    $(document).ready(function() {
      window.App = new window.Router();
      Backbone.history.start();
      window.App.handleResize();
    });
  }

}());
