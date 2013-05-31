/*jslint indent: 2, stupid: true, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, window, exports, Backbone, EJS, $, arangoHelper */

var collectionView = Backbone.View.extend({
  el: '#modalPlaceholder',
  initialize: function () {
  },

  template: new EJS({url: 'js/templates/collectionView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    $('#change-collection').modal('show');
    $('#change-collection').on('hidden', function () {
    });
    $('#change-collection').on('shown', function () {
      $('#change-collection-name').focus();
    });
    this.fillModal();
    $('.modalTooltips').tooltip({
      placement: "left"
    });

    return this;
  },
  events: {
    "click #save-modified-collection"     :    "saveModifiedCollection",
    "hidden #change-collection"           :    "hidden",
    "click #delete-modified-collection"   :    "deleteCollection",
    "click #load-modified-collection"     :    "loadCollection",
    "click #unload-modified-collection"   :    "unloadCollection",
    "keydown #change-collection-name"     :    "listenKey",
    "keydown #change-collection-size"     :    "listenKey"
  },
  listenKey: function(e) {
    if (e.keyCode === 13) {
      this.saveModifiedCollection();
    }
  },
  hidden: function () {
    window.App.navigate("#", {trigger: true});
  },
  fillModal: function() {
    try {
      this.myCollection = window.arangoCollectionsStore.get(this.options.colId).attributes;
    }
    catch (e) {
      // in case the collection cannot be found or something is not present (e.g. after a reload)
      window.App.navigate("#");
      return;
    }

    $('#change-collection-name').val(this.myCollection.name);
    $('#change-collection-id').text(this.myCollection.id);
    $('#change-collection-type').text(this.myCollection.type);
    $('#change-collection-status').text(this.myCollection.status);

    if (this.myCollection.status === 'unloaded') {
      $('#colFooter').append(
        '<button id="load-modified-collection" class="btn" style="margin-right: 10px">Load</button>'
      );
      $('#collectionSizeBox').hide();
      $('#collectionSyncBox').hide();
    }
    else if (this.myCollection.status === 'loaded') {
      $('#colFooter').append(
        '<button id="unload-modified-collection"'+
        'class="btn" style="margin-right: 10px">Unload</button>'
      );
      var data = window.arangoCollectionsStore.getProperties(this.options.colId, true);
      this.fillLoadedModal(data);
    }
  },
  fillLoadedModal: function (data) {
    $('#collectionSizeBox').show();
    $('#collectionSyncBox').show();
    if (data.waitForSync === false) {
      $('#change-collection-sync').val('false');
    }
    else {
      $('#change-collection-sync').val('true');
    }
    var calculatedSize = data.journalSize / 1024 / 1024;
    $('#change-collection-size').val(calculatedSize);
    $('#change-collection').modal('show');
  },
  saveModifiedCollection: function() {
    var newname = $('#change-collection-name').val();
    if (newname === '') {
      arangoHelper.arangoError('No collection name entered!');
      return 0;
    }

    var collid = this.getCollectionId();
    var status = this.getCollectionStatus();

    if (status === 'loaded') {
      var result;
      if (this.myCollection.name !== newname) {
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

      if (result === true) {
        arangoHelper.arangoNotification("Collection renamed");
      }

      if (result !== true) {
        if (result !== undefined) {
          arangoHelper.arangoError("Collection error: " + result);
          return 0;
        }
      }

      if (changeResult !== true) {
        arangoHelper.arangoNotification("Collection error: " + changeResult);
        return 0;
      }

      if (changeResult === true) {
        arangoHelper.arangoNotification("Saved collection properties");
          window.arangoCollectionsStore.fetch({
            success: function () {
              window.collectionsView.render();
            }
          });
          this.hideModal();
      }
    }
    else if (status === 'unloaded') {
      if (this.myCollection.name !== newname) {
        var result2 = window.arangoCollectionsStore.renameCollection(collid, newname);
        if (result2 === true) {

          window.arangoCollectionsStore.fetch({
            success: function () {
              window.collectionsView.render();
            }
          });
          this.hideModal();
          arangoHelper.arangoNotification("Collection renamed");
        }
        else {
          arangoHelper.arangoError("Collection error: " + result2);
        }
      }
      else {
        //arangoHelper.arangoNotification("No changes.");
        this.hideModal();
      }
    }
  },
  getCollectionId: function () {
    return this.myCollection.id;
  },
  getCollectionStatus: function () {
    return this.myCollection.status;
  },
  unloadCollection: function () {
    var collid = this.getCollectionId();
    window.arangoCollectionsStore.unloadCollection(collid);
    this.hideModal();
  },
  loadCollection: function () {
    var collid = this.getCollectionId();
    window.arangoCollectionsStore.loadCollection(collid);
    this.hideModal();
  },
  hideModal: function () {
    $('#change-collection').modal('hide');
  },
  deleteCollection: function () {
    var self = this;
    var collName = self.myCollection.name;
    var returnval = window.arangoCollectionsStore.deleteCollection(collName);
    if (returnval === true) {
      arangoHelper.arangoNotification('Collection deleted successfully.');
    }
    else {
      arangoHelper.arangoError('Could not delete collection.');
    }
    self.hideModal();
  }

});
