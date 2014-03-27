/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true*/
/*global Backbone, $, _, window, templateEngine*/

(function() {
  "use strict";

  window.GraphManagementView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate("graphManagementView.ejs"),

    events: {
      "click #deleteGraph"                  : "deleteGraph",
      "click .icon_arangodb_settings2"      : "editGraph",
      "click .icon_arangodb_info"           : "info",
      "click #createGraph"                  : "addNewGraph",
      "keyup #graphManagementSearchInput"   : "search",
      "click #graphManagementSearchSubmit"  : "search"
    },

    addNewGraph: function(e) {
      e.preventDefault();
      window.App.navigate("graphManagement/add", {trigger: true});
    },

    deleteGraph: function(e) {
      $('#editGraphModal').modal('hide');
      var key = this.graphToEdit;
      window.App.navigate("graphManagement/delete/" + key, {trigger: true});
    },

    render: function() {
      this.collection.fetch({
        async: false
      });
      $(this.el).html(this.template.render({
        graphs: this.collection,
        searchString : ''
      }));
      return this;
    },

    editGraph : function(e) {
      this.collection.fetch();
      this.graphToEdit = this.evaluateGraphName($(e.currentTarget).attr("id"), '_settings');
      $('#editGraphModal').modal('show');
      var graph = this.collection.findWhere({_key: this.graphToEdit});
      $('#editGraphName').html(this.graphToEdit);
      $('#editVertices').html(graph.get("vertices"));
      $('#editEdges').html(graph.get("edges"));
    },

    info : function(e) {
      this.collection.fetch();
      $('#infoGraphModal').modal('show');
      var graph = this.collection.findWhere(
        {_key: this.evaluateGraphName($(e.currentTarget).attr("id"), '_info')}
      );
      $('#infoGraphName').html(graph.get('_key'));
      $('#infoVertices').html(graph.get('vertices'));
      $('#infoEdges').html(graph.get('edges'));
    },

    evaluateGraphName : function(str, substr) {
      var index = str.lastIndexOf(substr);
      return str.substring(0, index);
    },

    search: function() {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#graphManagementSearchInput');
      searchString = $("#graphManagementSearchInput").val();
      reducedCollection = this.collection.filter(
        function(u) {
          return u.get("_key").indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        graphs        : reducedCollection,
        searchString  : searchString
      }));

      //after rendering, get the "new" element
      searchInput = $('#graphManagementSearchInput');
      //set focus on end of text in input field
      strLength= searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    }


  });

}());
