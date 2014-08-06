/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone, console */
(function() {
  "use strict";

  window.AutomaticRetryCollection = Backbone.Collection.extend({

    _retryCount: 0,


    checkRetries: function() {
      var self = this;
      this.updateUrl();
      if (this._retryCount > 10) {
        window.setTimeout(function() {
          self._retryCount = 0;
        }, 10000);
        window.App.clusterUnreachable();
        return false;
      }
      return true;
    },

    successFullTry: function() {
      this._retryCount = 0;
    },

    failureTry: function(retry, err) {
      if (err.status === 401) {
        window.App.requestAuth();
      } else {
        window.App.clusterPlan.rotateCoordinator();
        this._retryCount++;
        retry();
      }
    }

  });
}());
