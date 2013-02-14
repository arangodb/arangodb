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
    "click #save-modified-collection"    :    "saveNewCollection",
    "hidden #change-collection"          :    "hidden",
  },
  hidden: function () {
    window.location.hash = "#";
  },
  fillModal: function() {
    myCollection = window.arangoCollectionsStore.get(this.options.colId).attributes;
    console.log(myCollection);
    $('#change-collection-name').val(myCollection.name);
    $('#change-collection-id').val(myCollection.id);
    $('#change-collection-type').val(myCollection.type);
    $('#change-collection-status').val(myCollection.status);

    if (myCollection.status == 'loaded') {
      $('#collectionSizeBox').show();
      $('#collectionSyncBox').show();
      var data = window.arangoCollectionsStore.getProperties(this.options.colId);
      console.log(data);
      if (data.waitForSync == false) {
        $('#change-collection-sync').val('false');
      }
      else {
        $('#change-collection-sync').val('true');
      }
      $('#change-collection-size').val(data.journalSize);
      $('#change-collection').modal('show')
    }
    else if (myCollection.status == 'unloaded') {
      $('#collectionSizeBox').hide();
      $('#collectionSyncBox').hide();
    }
  },
  saveNewCollection: function() {
    console.log("atm missing");
    return false;
  },
  saveModifiedCollection: function() {

  }

});
