/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, window, exports, Backbone, EJS, $, templateEngine, arangoHelper*/

(function() {
  "use strict";

  window.CollectionListItemView = Backbone.View.extend({

    tagName: "div",
    className: "tile",
    template: templateEngine.createTemplate("collectionsItemView.ejs"),

    initialize: function () {
      var self = this;
      $.ajax(
        "cluster/amICoordinator",
        {async: false}
      ).done(
        function(d) {
          self.isCoordinator = d;
        }
      );
    },
    events: {
      'click .iconSet.icon_arangodb_settings2': 'createEditPropertiesModal',
      'click .pull-left' : 'noop',
      'click .icon_arangodb_settings2' : 'editProperties',
//      'click #editCollection' : 'editProperties',
      'click .spanInfo' : 'showProperties',
      'click': 'selectCollection'
    },
    render: function () {
      $(this.el).html(this.template.render({
        model: this.model
      }));
      $(this.el).attr('id', 'collection_' + this.model.get('name'));
      return this;
    },

    editProperties: function (event) {
      event.stopPropagation();
      this.createEditPropertiesModal();
    },

    showProperties: function(event) {
      event.stopPropagation();
      window.App.navigate(
        "collectionInfo/" + encodeURIComponent(this.model.get("id")), {trigger: true}
      );
    },
    
    selectCollection: function() {
      window.App.navigate(
        "collection/" + encodeURIComponent(this.model.get("name")) + "/documents/1", {trigger: true}
      );
    },
    
    noop: function(event) {
      event.stopPropagation();
    },

    unloadCollection: function () {
      window.arangoCollectionsStore.unloadCollection(this.model.get('id'));
      window.modalView.hide();
    },
    loadCollection: function () {
      window.arangoCollectionsStore.loadCollection(this.model.get('id'));
      window.modalView.hide();
    },
    deleteCollection: function () {
      var collName = this.model.get('name');
      var returnval = window.arangoCollectionsStore.deleteCollection(collName);
      if (returnval === false) {
        arangoHelper.arangoError('Could not delete collection.');
      }
      window.modalView.hide();
    },
    saveModifiedCollection: function() {
      var newname;
      if (this.isCoordinator) {
        newname = this.model.get('name');
      }
      else {
        newname = $('#change-collection-name').val();
        if (newname === '') {
          arangoHelper.arangoError('No collection name entered!');
          return 0;
        }
      }

      var collid = this.model.get('id');
      var status = this.model.get('status');

      if (status === 'loaded') {
        var result;
        if (this.model.get('name') !== newname) {
          result = window.arangoCollectionsStore.renameCollection(collid, newname);
        }

        var wfs = $('#change-collection-sync').val();
        var journalSize;
        try {
          journalSize = JSON.parse($('#change-collection-size').val() * 1024 * 1024);
        }
        catch (e) {
          arangoHelper.arangoError('Please enter a valid number');
          return 0;
        }
        var changeResult = window.arangoCollectionsStore.changeCollection(collid, wfs, journalSize);

        if (result !== true) {
          if (result !== undefined) {
            arangoHelper.arangoError("Collection error: " + result);
            return 0;
          }
        }

        if (changeResult !== true) {
          arangoHelper.arangoNotification("Collection error", changeResult);
          return 0;
        }

        if (changeResult === true) {
          window.arangoCollectionsStore.fetch({
            success: function () {
              window.collectionsView.render();
            }
          });
          window.modalView.hide();
        }
      }
      else if (status === 'unloaded') {
        if (this.model.get('name') !== newname) {
          var result2 = window.arangoCollectionsStore.renameCollection(collid, newname);
          if (result2 === true) {

            window.arangoCollectionsStore.fetch({
              success: function () {
                window.collectionsView.render();
              }
            });
            window.modalView.hide();
          }
          else {
            arangoHelper.arangoError("Collection error: " + result2);
          }
        }
        else {
          window.modalView.hide();
        }
      }
    },



    //modal dialogs

    createEditPropertiesModal: function() {
      var buttons = [],
        tableContent = [];

      if (this.isCoordinator) {
/*
        <th class="collectionTh"><input type="text" id="change-collection-name" name="name" value="" readonly="readonly" /></th>
*/

      } else {
//        <input type="text" id="change-collection-name" name="name" value=""/></th>
          //(id, label, value, info, placeholder, mandatory)
        tableContent.push(
          window.modalView.createTextEntry(
            "change-collection-name", "Name", this.model.get('name'), false, "", true
          )
        );
      }
      // TODO: needs to be refactored. move getProperties into model
      var journalSize = this.model.collection.getProperties(this.model.get('id')).journalSize;
      journalSize = journalSize/(1024*1024);
      tableContent.push(
        window.modalView.createTextEntry(
          "change-collection-size", "Journal size", journalSize, false, "", true
        )
      );

      var wfs = this.model.collection.getProperties(this.model.get('id')).waitForSync;

      tableContent.push(
        window.modalView.createSelectEntry(
          "change-collection-sync",
          "Wait for sync",
          wfs,
          "Synchronise to disk before returning from a create or update of a document.",
          [{value: false, label: "No"}, {value: true, label: "Yes"}]        )
      );
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "ID", this.model.get('id'), ""
        )
      );
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "Type", this.model.get('type'), ""
        )
      );
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "Status", this.model.get('status'), ""
        )
      );
      if(this.model.get('status') === "loaded") {
        buttons.push(
          window.modalView.createNotificationButton(
            "Unload",
            this.unloadCollection.bind(this)
          )
        );
      } else {
        buttons.push(
          window.modalView.createNotificationButton(
            "Load",
            this.loadCollection.bind(this)
          )
        );

      }
      buttons.push(
        window.modalView.createDeleteButton(
          "Delete",
          this.deleteCollection.bind(this)
        )
      );
      buttons.push(
        window.modalView.createSuccessButton(
          "Save",
          this.saveModifiedCollection.bind(this)
        )
      );

      window.modalView.show(
        "modalTable.ejs",
        "Modify Collection",
        buttons,
        tableContent
      );
    }

  });
}());
