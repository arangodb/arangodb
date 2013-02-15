var collectionView = Backbone.View.extend({
  el: '#modalPlaceholder',
  initialize: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/collectionView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    $('#change-collection').modal('show');
    $('#change-collection').on('hidden', function () {
    });
    this.fillModal();

    return this;
  },
  events: {
    "click #save-modified-collection"    :    "saveModifiedCollection",
    "hidden #change-collection"          :    "hidden",
    "click #delete-modified-collection"  :    "deleteCollection"
  },
  hidden: function () {
    window.location.hash = "#";
  },
  fillModal: function() {
    myCollection = window.arangoCollectionsStore.get(this.options.colId).attributes;
    $('#change-collection-name').val(myCollection.name);
    $('#change-collection-id').val(myCollection.id);
    $('#change-collection-type').val(myCollection.type);
    $('#change-collection-status').val(myCollection.status);

    if (myCollection.status == 'unloaded') {
      $('#collectionSizeBox').hide();
      $('#collectionSyncBox').hide();
    }
    else if (myCollection.status == 'loaded') {
      var data = window.arangoCollectionsStore.getProperties(this.options.colId, true);
      this.fillLoadedModal(data);
    }
  },
  fillLoadedModal: function (data) {
    $('#collectionSizeBox').show();
    $('#collectionSyncBox').show();
    if (data.waitForSync == false) {
      $('#change-collection-sync').val('false');
    }
    else {
      $('#change-collection-sync').val('true');
    }
    $('#change-collection-size').val(data.journalSize);
    $('#change-collection').modal('show')
  },
  saveModifiedCollection: function() {
  },
  deleteCollection: function () {
    var self = this;
    var collName = $('#change-collection-name').val();

    $.ajax({
      type: 'DELETE',
      url: "/_api/collection/" + collName,
      success: function () {
        $('#change-collection').modal('hide');
      },
      error: function () {
        $('#change-collection').modal('hide');
        alert('Error');
      }
    });
  }

});
