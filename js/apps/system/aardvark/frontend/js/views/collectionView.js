/*jslint indent: 2, stupid: true, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, window, exports, Backbone, EJS, $, arangoHelper */

var collectionView = Backbone.View.extend({
  el: '#modalPlaceholder',
  initialize: function () {
  },

  template: new EJS({url: 'js/templates/collectionView.ejs'}),

  render: function() {
    var self = this;
    $(this.el).html(this.template.text);
    $('#change-collection').modal('show');
    $('#change-collection').on('hidden', function () {
    });
    $('#change-collection').on('shown', function () {
      $('#change-collection-name').focus();
    });
    this.fillModal();

    $('.modalTooltips, .glyphicon-info-sign').tooltip({
      placement: "left"
    });

    $('#collectionTab a').click(function (e) {
      e.preventDefault();
      $(this).tab('show');
      if ($(this).attr('href') === '#editIndex') {
        self.resetIndexForms();
      }
    });

    return this;
  },
  events: {
    "click #save-modified-collection"       :    "saveModifiedCollection",
    "hidden #change-collection"             :    "hidden",
    "click #delete-modified-collection"     :    "deleteCollection",
    "click #load-modified-collection"       :    "loadCollection",
    "click #unload-modified-collection"     :    "unloadCollection",
    "click #confirmDeleteCollection"        :    "confirmDeleteCollection",
    "click #abortDeleteCollection"          :    "abortDeleteCollection",
    "keydown #change-collection-name"       :    "listenKey",
    "keydown #change-collection-size"       :    "listenKey",
    "click #editIndex .glyphicon-plus-sign"     :    "toggleNewIndexView",
    "click #editIndex .glyphicon-remove-circle" :    "toggleNewIndexView",
    "change #newIndexType" : "selectIndexType",
    "click #createIndex" : "createIndex",
    "click .deleteIndex" : "deleteIndex"
  },
  listenKey: function(e) {
    if (e.keyCode === 13) {
      this.saveModifiedCollection();
    }
  },
  hidden: function () {
    window.App.navigate("#collections", {trigger: true});
  },
  toggleNewIndexView: function () {
    $('#indexEditView').toggle("fast");
    $('#newIndexView').toggle("fast");
    this.resetIndexForms();
  },
  selectIndexType: function () {
    $('.newIndexClass').hide();
    var type = $('#newIndexType').val();
    $('#newIndexType'+type).show();
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
      $('#colFooter').prepend(
        '<button id="load-modified-collection" class="btn btn-notification">Load</button>'
      );
      $('#collectionSizeBox').hide();
      $('#collectionSyncBox').hide();
      $('#tab-content-collection-edit tab-pane').css("border-top",0);
    }
    else if (this.myCollection.status === 'loaded') {
      $('#colFooter').prepend(
        '<button id="unload-modified-collection"'+
        'class="btn btn-notification">Unload</button>'
      );
      var data = window.arangoCollectionsStore.getProperties(this.options.colId, true);
      this.fillLoadedModal(data);
    }
  },
  stringToArray: function (fieldString) {
    var fields = [];
    fieldString.split(',').forEach(function(field){
      field = field.replace(/(^\s+|\s+$)/g,'');
      if (field !== '') {
        fields.push(field);
      }
    });
    return fields;
  },
  checkboxToValue: function (id) {
    var checked = $(id).is('checked');
    return checked;
  },
  createIndex: function (e) {
    //e.preventDefault();
    var self = this;
    var collection = this.myCollection.name;
    var indexType = $('#newIndexType').val();
    var result;
    var postParameter = {};
    var fields;
    var unique;

    switch(indexType) {
      case 'Cap':
        var size = parseInt($('#newCapSize').val(), 10) || 0;
        var byteSize = parseInt($('#newCapByteSize').val(), 10) || 0;
        postParameter = {
          type: 'cap',
          size: size,
          byteSize: byteSize
        };
        break;
      case 'Geo':
        //HANDLE ARRAY building
        fields = $('#newGeoFields').val();
        var geoJson = self.checkboxToValue('#newGeoJson');
        var constraint = self.checkboxToValue('#newGeoConstraint');
        var ignoreNull = self.checkboxToValue('#newGeoIgnoreNull');
        postParameter = {
          type: 'geo',
          fields: self.stringToArray(fields),
          geoJson: geoJson,
          constraint: constraint,
          ignoreNull: ignoreNull
        };
        break;
      case 'Hash':
        fields = $('#newHashFields').val();
        unique = self.checkboxToValue('#newHashUnique');
        postParameter = {
          type: 'hash',
          fields: self.stringToArray(fields),
          unique: unique
        };
        break;
      case 'Fulltext':
        fields = ($('#newFulltextFields').val());
        var minLength =  parseInt($('#newFulltextMinLength').val(), 10) || 0;
        postParameter = {
          type: 'fulltext',
          fields: self.stringToArray(fields),
          minLength: minLength
        };
        break;
      case 'Skiplist':
        fields = $('#newSkiplistFields').val();
        unique = self.checkboxToValue('#newSkiplistUnique');
        postParameter = {
          type: 'skiplist',
          fields: self.stringToArray(fields),
          unique: unique
        };
        break;
    }
    result = window.arangoCollectionsStore.createIndex(collection, postParameter);
    if (result === true) {
      $('#collectionEditIndexTable tr').remove();
      self.getIndex();
      self.toggleNewIndexView();
      self.resetIndexForms();
    }
    else {
      arangoHelper.arangoError("Could not create index");
    }
  },

  resetIndexForms: function () {
    $('#editIndex input').val('').prop("checked", false);
    $('#newIndexType').val('Cap').prop('selected',true);
  },

  deleteIndex: function (e) {
    var collection = this.myCollection.name;
    var id = $(e.currentTarget).parent().parent().first().children().first().text();
    var result = window.arangoCollectionsStore.deleteIndex(collection, id);
    if (result === true) {
      $(e.currentTarget).parent().parent().remove();
    } 
    else {
      arangoHelper.arangoError("Could not delete index");
    }
  },

  getIndex: function () {
    this.index = window.arangoCollectionsStore.getIndex(this.options.colId, true);
    var cssClass = 'collectionInfoTh modal-text';
    if (this.index) {
      var fieldString = '';
      var indexId = '';
      var actionString = '';

      $.each(this.index.indexes, function(k,v) {

        if (v.type === 'primary' || v.type === 'edge') {
          actionString = '<span class="glyphicon glyphicon-ban-circle" ' +
                         'data-original-title="No action"></span>';
        }
        else {
          actionString = '<span class="deleteIndex glyphicon glyphicon-minus-sign" ' +
                         'data-original-title="Delete index"></span>';
        }

        if (v.fields !== undefined) {
          fieldString = v.fields.join(", ");
        }

        //cut index id
        var position = v.id.indexOf('/');
        var indexId = v.id.substr(position+1, v.id.length);

        $('#collectionEditIndexTable').append(
          '<tr>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + indexId + '</th>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + v.type + '</th>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + v.unique + '</th>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + fieldString + '</th>'+
            '<th class=' + JSON.stringify(cssClass) + '>' + actionString + '</th>'+
          '</tr>'
        );
      });
    }
  },

  fillLoadedModal: function (data) {

    //show tabs & render figures tab-view
    $('#change-collection .nav-tabs').css("visibility","visible");
    this.getIndex();

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
    $('#reallyDeleteColDiv').show();
  },
  abortDeleteCollection: function() {
    $('#reallyDeleteColDiv').hide();
  },
  confirmDeleteCollection: function () {
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
