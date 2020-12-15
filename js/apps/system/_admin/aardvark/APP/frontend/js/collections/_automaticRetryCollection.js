/* global window, Backbone */
(function () {
  'use strict';

  window.AutomaticRetryCollection = Backbone.Collection.extend({
    _retryCount: 0,

    checkRetries: function () {
      var self = this;
      this.updateUrl();
      if (this._retryCount > 10) {
        window.setTimeout(function () {
          self._retryCount = 0;
        }, 10000);
        window.App.clusterUnreachable();
        return false;
      }
      return true;
    },

    successFullTry: function () {
      this._retryCount = 0;
    },

    failureTry: function (retry, ignore, err) {
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
