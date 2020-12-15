/* global window, Backbone */
(function () {
  'use strict';

  window.ClusterServer = Backbone.Model.extend({
    defaults: {
      name: '',
      address: '',
      role: '',
      status: 'ok'
    },

    idAttribute: 'name',
    /*
    url: "/_admin/aardvark/cluster/DBServers"

    updateUrl: function() {
      this.url = window.getNewRoute("DBServers")
    },
    */
    forList: function () {
      return {
        name: this.get('name'),
        address: this.get('address'),
        status: this.get('status')
      };
    }

  });
}());
