/* global window, Backbone */
(function () {
  'use strict';

  window.ClusterCoordinator = Backbone.Model.extend({
    defaults: {
      'name': '',
      'id': '',
      'status': 'ok',
      'address': '',
      'protocol': ''
    },

    idAttribute: 'name',
    /*
    url: "/_admin/aardvark/cluster/Coordinators"

    updateUrl: function() {
      this.url = window.getNewRoute("Coordinators")
    },
    */
    forList: function () {
      return {
        name: this.get('name'),
        status: this.get('status'),
        url: this.get('url')
      };
    }

  });
}());
