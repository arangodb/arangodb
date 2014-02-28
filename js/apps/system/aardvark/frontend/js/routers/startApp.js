/*jslint indent: 2, nomen: true, maxlen: 100, white: true, */
/*global window, $, Backbone, document */ 

(function() {
  "use strict";

  $.get("cluster/amIDispatcher", function(data) {
    if (data) {
      var url = window.location.origin;
      url += window.location.pathname;
      url = url.replace("index", "cluster");
      window.location.replace(url);
    }
  });

  $(document).ready(function() {
    window.App = new window.Router();
    Backbone.history.start();
    window.App.handleResize();
  });

}());
