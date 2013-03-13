var newCollectionView = Backbone.View.extend({
  el: '#modalPlaceholder',
  initialize: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/newCollectionView.ejs'}),

  render: function() {
    var self = this;
    $(this.el).html(this.template.text);
    $('#add-collection').modal('show');
    $('#add-collection').on('hidden', function () {
      self.hidden();
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
  },

  hidden: function () {
    window.location.hash = "#collection/";
  },

  saveNewCollection: function() {
    var self = this;

    var collName = $('#new-collection-name').val();
    var collSize = $('#new-collection-size').val();
    var collType = $('#new-collection-type').val();
    var collSync = $('#new-collection-sync').val();
    var isSystem = (collName.substr(0, 1) === '_');
    var wfs = (collSync == "true");

    if (collSize == '') {
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
    if (collName == '') {
      arangoHelper.arangoError('No collection name entered!');
      return 0;
    }

    var returnobj = window.arangoCollectionsStore.newCollection(collName, wfs, isSystem, journalSizeString, collType);
    if (returnobj.status === true) {
      self.hidden();
      $("#add-collection").modal('hide');
      arangoHelper.arangoNotification("Collection created");
    }
    else {
      self.hidden();
      $("#add-collection").modal('hide');
      arangoHelper.arangoError(returnobj.errorMessage);
    }
    window.arangoCollectionsStore.fetch({
      success: function () {
        window.collectionsView.render();
      }
    });
  }

});
