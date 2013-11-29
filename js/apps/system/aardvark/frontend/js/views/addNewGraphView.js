/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine*/

(function() {
  "use strict";

  window.AddNewGraphView = Backbone.View.extend({
    el: '#modalPlaceholder',
    template: templateEngine.createTemplate("addNewGraphView.ejs"),

    initilize: function() {
      this.graphs = this.options.graphs;
    },

    events: {
    },

    render: function() {
      this.collection.fetch({
        async: false
      });
      this.graphs.fetch({
        async: false
      });
      $(this.el).html(this.template.render({
        collections: this.collection,
        graphs: this.graphs
      }));
      $('#change-foxx').modal('show');
      $('.modalTooltips').tooltip({
        placement: "left"
      });
      return this;
    }
  });

}());
