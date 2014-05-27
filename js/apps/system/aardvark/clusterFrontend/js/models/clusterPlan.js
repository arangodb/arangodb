/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global window, Backbone, $, _*/
(function() {
  "use strict";

  window.ClusterPlan = Backbone.Model.extend({

    defaults: {
    },

    url: "cluster/plan",

    idAttribute: "config",

    getVersion: function() {
      var v = this.get("version");
      return v || "2.0";
    },

    getCoordinator: function() {
      if (this._coord) {
        return this._coord[
          this._lastStableCoord
        ];
      }
      var tmpList = [];
      var i,j,r,l;
      r = this.get("runInfo");
      if (!r) {
        return;
      }
      j = r.length-1;
      while (j > 0) {
        if(r[j].isStartServers) {
          l = r[j];
          if (l.endpoints) {
            for (i = 0; i < l.endpoints.length;i++) {
              if (l.roles[i] === "Coordinator") {
                tmpList.push(l.endpoints[i]
                  .replace("tcp://","http://")
                  .replace("ssl://", "https://")
                );
              }
            }
          }
        }
        j--;
      }
      this._coord = tmpList;
      this._lastStableCoord = Math.floor(Math.random() * this._coord.length);
    },

    rotateCoordinator: function() {
      var last = this._lastStableCoord, next;
      if (this._coord.length > 1) {
        do {
          next = Math.floor(Math.random() * this._coord.length);
        } while (next === last);
        this._lastStableCoord = next;
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
