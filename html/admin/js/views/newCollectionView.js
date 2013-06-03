/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, window, arangoHelper*/

var newCollectionView = Backbone.View.extend({
  el: '#modalPlaceholder',
  initialize: function () {
  },

  template: new EJS({url: 'js/templates/newCollectionView.ejs'}),

  render: function() {
    var self = this;
    $(this.el).html(this.template.text);
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
  },

  saveNewCollection: function(a) {
    if (window.location.hash !== '#new') {
      return;
    }

    var self = this;

    var collName = $('#new-collection-name').val();
    var collSize = $('#new-collection-size').val();
    var collType = $('#new-collection-type').val();
    var collSync = $('#new-collection-sync').val();
    var isSystem = (collName.substr(0, 1) === '_');
    var wfs = (collSync === "true");
    var journalSizeString;

    if (collSize === '') {
      journalSizeString = '';
    }
    else {
      try {
        collSize = JSON.parse(collSize) * 1024 * 1024;
        journalSizeString = ', "journalSize":' + collSize;
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
      collName, wfs, isSystem, journalSizeString, collType
    );
    if (returnobj.status === true) {
      self.hidden();
      $("#add-collection").modal('hide');
      arangoHelper.arangoNotification("Collection created");

      window.App.navigate("collection/" + collName + "/documents/1", {trigger: true});
    }
    else {
      arangoHelper.arangoError(returnobj.errorMessage);
    }
  }

});
