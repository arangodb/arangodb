/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, window, arangoHelper, templateEngine, _*/

(function (){
  "use strict";
  window.newCollectionView = Backbone.View.extend({
    el: '#modalPlaceholder',
    initialize: function () {
      var self = this;
      $.ajax("cluster/amICoordinator", {
       async: false  
     }).done(function(d) {
        self.isCoordinator = d;
      });
    },

    template: templateEngine.createTemplate("newCollectionView.ejs"),

    render: function() {
      var self = this;
      $(this.el).html(this.template.render({
        isCoordinator: this.isCoordinator
      }));
      if (this.isCoordinator) {
        $("#new-collection-shardBy").select2({
          tags: [],
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: "336px",
          maximumSelectionSize: 8
        });
      }
      $('#add-collection').modal('show');
      $('#add-collection').on('hidden', function () {
        self.hidden();
      });
      $('#add-collection').on('shown', function () {
        $('#new-collection-name').focus();
      });

      $('#edgeFrom').hide();
      $('#edgeTo').hide();
      $('.modalTooltips').tooltip({
        placement: "left"
      });

      return this;
    },

    events: {
      "click #save-new-collection" : "saveNewCollection",
      "keydown #new-collection-name": "listenKey",
      "keydown #new-collection-size": "listenKey"
    },

    listenKey: function(e) {
      if (e.keyCode === 13) {
        this.saveNewCollection();
      }
    },

    hidden: function () {
      window.App.navigate("#collections", {trigger: true});
    },

    saveNewCollection: function(a) {
      var self = this;

      var collName = $('#new-collection-name').val();
      var collSize = $('#new-collection-size').val();
      var collType = $('#new-collection-type').val();
      var collSync = $('#new-collection-sync').val();
      var shards = 1;
      var shardBy = [];
      if (this.isCoordinator) {
        shards = $('#new-collection-shards').val();
        if (shards === "") {
          shards = 1;
        }
        shards = parseInt(shards, 10);
        if (shards < 1) {
          arangoHelper.arangoError(
            "Number of shards has to be an integer value greater or equal 1"
          );
          return 0;
        }
        shardBy = _.pluck($('#new-collection-shardBy').select2("data"), "text");
        if (shardBy.length === 0) {
          shardBy.push("_key");
        }
      }
      var isSystem = (collName.substr(0, 1) === '_');
      var wfs = (collSync === "true");
      if (collSize > 0) {
        try {
          collSize = JSON.parse(collSize) * 1024 * 1024;
        }
        catch (e) {
          arangoHelper.arangoError('Please enter a valid number');
          return 0;
        }
      }
      if (collName === '') {
        arangoHelper.arangoError('No collection name entered!');
        return 0;
      }

      var returnobj = window.arangoCollectionsStore.newCollection(
        collName, wfs, isSystem, collSize, collType, shards, shardBy
      );
      if (returnobj.status === true) {
        self.hidden();
        $("#add-collection").modal('hide');

        window.App.navigate("collection/" + collName + "/documents/1", {trigger: true});
      }
      else {
        arangoHelper.arangoError(returnobj.errorMessage);
      }
    }

  });
}());
