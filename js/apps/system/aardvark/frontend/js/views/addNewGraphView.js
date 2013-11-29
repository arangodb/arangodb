/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine*/

(function() {
  "use strict";

  window.AddNewGraphView = Backbone.View.extend({
    el: '#modalPlaceholder',
    template: templateEngine.createTemplate("addNewGraphView.ejs"),

    initialize: function() {
      this.graphs = this.options.graphs;
    },

    events: {
      "click #cancel": "hide",
      "hidden": "hidden",
      "click #createGraph": "createGraph"
    },

    createGraph: function() {
      this.graphs.create({
        _key: $("#newGraphName").val(),
        vertices: $("#newGraphVertices").val(),
        edges: $("#newGraphEdges").val()
      });
      this.hide();
    },

    hide: function() {
      $('#add-graph').modal('hide');
    },

    hidden: function () {
      this.undelegateEvents();
      window.App.navigate("graphManagement", {trigger: true});
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
      $('#add-graph').modal('show');
      $('.modalTooltips').tooltip({
        placement: "left"
      });
      return this;
    }
  });

}());
