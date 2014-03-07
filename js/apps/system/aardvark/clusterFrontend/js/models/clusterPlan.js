/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterPlan = Backbone.Model.extend({

    defaults: {
    },

    url: "cluster/plan",

    idAttribute: "config",

    getCoordinator: function() {
      if (this._coord) {
        return this._coord;
      }
      var i,j,r;
      r = this.get("runInfo");
      if (!r) {
        console.log(this);
        return;
      }
      j = r.length-1;
      while (j > 0) {
        if(r[j].isStartServers) {
          var l = r[j];
          if (l.endpoints) {
            for (i = 0; i < l.endpoints.length;i++) {
              if (l.roles[i] === "Coordinator") {
                this._coord = l.endpoints[i]
                  .replace("tcp://","http://")
                  .replace("ssl://", "https://");
                return this._coord;
              }
            }
          }
        }
        j--;
      }
    },

    isAlive : function() {
      var result = false;
      $.ajax({
        cache: false,
        type: "GET",
        async: false, // sequential calls!
        url: "cluster/healthcheck",
        success: function(data) {
          result = data;
        },
        error: function(data) {
        }
      });
      return result;
    },

    storeCredentials: function(name, passwd) {
      var self = this;
      $.ajax({
        url: "cluster/plan/credentials",
        type: "PUT",
        data: JSON.stringify({
          user: name,
          passwd: passwd
        }),
        async: false
      }).done(function() {
        self.fetch();
      });
    },

    isSymmetricSetup: function() {
      var config = this.get("config");
      var count = _.size(config.dispatchers);
      return count === config.numberOfCoordinators
        && count === config.numberOfDBservers;
    },

    isTestSetup: function() {
      return _.size(this.get("config").dispatchers) === 1;
    }

  });
}());
