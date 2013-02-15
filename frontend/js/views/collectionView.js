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
    "click #save-modified-collection"     :    "saveModifiedCollection",
    "hidden #change-collection"           :    "hidden",
    "click #delete-modified-collection"   :    "deleteCollection",
    "click #load-modified-collection"     :    "loadCollection",
    "click #unload-modified-collection" :    "unloadCollection"
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
      $('#colFooter').append('<button id="load-modified-collection" class="btn">Load</button>');
      $('#collectionSizeBox').hide();
      $('#collectionSyncBox').hide();
    }
    else if (myCollection.status == 'loaded') {
      $('#colFooter').append('<button id="unload-modified-collection" class="btn">Unload</button>');
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
  unloadCollection: function () {
    var collid = $('#change-collection-name').val();
    window.arangoCollectionsStore.unloadCollection(collid);
    $('#change-collection').modal('hide');
  },
  loadCollection: function () {
    var collid = $('#change-collection-name').val();
    window.arangoCollectionsStore.loadCollection(collid);
    $('#change-collection').modal('hide');
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
