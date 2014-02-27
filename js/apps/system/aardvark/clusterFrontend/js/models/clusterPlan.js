/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterPlan = Backbone.Model.extend({

    defaults: {
    },

    url: "cluster/plan",

    isNew : function() {
      return true;
    }

  });
}());
