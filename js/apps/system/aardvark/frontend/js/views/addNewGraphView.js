/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine, arangoHelper*/

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
      var _key = $("#newGraphName").val(),
        vertices = $("#newGraphVertices").val(),
        edges = $("#newGraphEdges").val(),
        self = this;

      if (!_key) {
        arangoHelper.arangoNotification(
          "A name for the graph has to be provided."
        );
        return;
      } 
      if (!vertices) {
        arangoHelper.arangoNotification(
          "A vertex collection has to be provided."
        );
        return;
      } 
      if (!edges) {
        arangoHelper.arangoNotification(
          "An edge collection has to be provided."
        );
        return;
      } 
      this.graphs.create({
        _key: _key,
        vertices: vertices,
        edges: edges
      }, {
        success: function() {
          self.hide(); 
        },
        error: function(err) {
          arangoHelper.arangoError(err.errorMessage);
        }
      });
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
      this.delegateEvents();
      $('#add-graph').modal('show');
      $('.modalTooltips').tooltip({
        placement: "left"
      });
      return this;
    }
  });

}());
