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
    "click #unload-modified-collection" :    "unloadCollection"
  },
  hidden: function () {
    window.location.hash = "#collection/";
  },
  fillModal: function() {
    this.myCollection = window.arangoCollectionsStore.get(this.options.colId).attributes;
    $('#change-collection-name').val(this.myCollection.name);
    $('#change-collection-id').text(this.myCollection.id);
    $('#change-collection-type').text(this.myCollection.type);
    $('#change-collection-status').text(this.myCollection.status);

    if (this.myCollection.status == 'unloaded') {
      $('#colFooter').append('<button id="load-modified-collection" class="arangoBtn">Load</button>');
      $('#collectionSizeBox').hide();
      $('#collectionSyncBox').hide();
    }
    else if (this.myCollection.status == 'loaded') {
      $('#colFooter').append('<button id="unload-modified-collection" class="arangoBtn">Unload</button>');
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
    var calculatedSize = data.journalSize / 1024 / 1024;
      $('#change-collection-size').val(calculatedSize);
    $('#change-collection').modal('show')
  },
  saveModifiedCollection: function() {
    var collid = this.getCollectionId();
    var status = this.getCollectionStatus();
    if (status === 'loaded') {
      var newname = $('#change-collection-name').val();
      if (this.myCollection.name !== newname) {
        window.arangoCollectionsStore.renameCollection(collid, newname );
      }

      var wfs = $('#change-collection-sync').val();
      var journalSize = JSON.parse($('#change-collection-size').val() * 1024 * 1024);
      window.arangoCollectionsStore.changeCollection(collid, wfs, journalSize);
      this.hideModal();
    }
    else if (status === 'unloaded') {
      var newname = $('#change-collection-name').val();
      if (this.myCollection.name !== newname) {
        window.arangoCollectionsStore.renameCollection(collid, newname );
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
    var collName = $('#change-collection-name').val();
    var returnval = window.arangoCollectionsStore.deleteCollection(collName);
    if (returnval === true) {
      self.hideModal();
    }
    else {
      self.hideModal();
      alert("Error");
    }
  }

});
