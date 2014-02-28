/*jslint indent: 2, stupid: true, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, window, exports, Backbone, EJS, $, arangoHelper, nv, d3*/
/*global templateEngine*/

(function() {
  "use strict";
  window.CollectionInfoView = Backbone.View.extend({
    el: '#modalPlaceholder',

    initialize: function () {
    },

    template: templateEngine.createTemplate("collectionInfoView.ejs"),

    render: function() {
      $(this.el).html(this.template.render({
        figuresData :this.data = window.arangoCollectionsStore.getFigures(this.options.colId, true)
      }));
      $('#show-collection').modal('show');
      this.fillModal();

      $("[data-toggle=tooltip]").tooltip();

      $('.modalInfoTooltips').tooltip({
        placement: "left"
      });

      $('#infoTab a').click(function (e) {
        e.preventDefault();
        $(this).tab('show');
        $(this).focus();
      });

      return this;
    },
    events: {
      "hidden #show-collection" : "hidden"
    },
    hidden: function () {
      window.App.navigate("#collections", {trigger: true});
    },

    setColId: function (colId) {
      this.options.colId = colId;
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

      $('#show-collection-name').text("Collection: " + this.myCollection.name);
      $('#show-collection-id').text(this.myCollection.id);
      $('#show-collection-type').text(this.myCollection.type);
      $('#show-collection-status').text(this.myCollection.status);

      if (this.myCollection.status === 'loaded') {
        this.data = window.arangoCollectionsStore.getFigures(this.options.colId, true);
        this.revision = window.arangoCollectionsStore.getRevision(this.options.colId, true);
        this.properties = window.arangoCollectionsStore.getProperties(this.options.colId, true);
        //remove 
        this.index = window.arangoCollectionsStore.getIndex(this.options.colId, true);
        this.fillLoadedModal();
      }
    },
    roundNumber: function(number, n) {
      var factor;
      factor = Math.pow(10,n);
      var returnVal = (Math.round(number * factor) / factor);
      return returnVal;
    },

    appendIndex: function () {
      var cssClass = 'collectionInfoTh modal-text';
      if (this.index) {
        var fieldString = '';
        $.each(this.index.indexes, function(k,v) {
          if (v.fields !== undefined) {
            fieldString = v.fields.join(", ");
          }

          //cut index id
          var position = v.id.indexOf('/');
          var indexId = v.id.substr(position+1, v.id.length);

          $('#collectionIndexTable').append(
            '<tr>'+
              '<th class=' + JSON.stringify(cssClass) + '>' + indexId + '</th>'+
              '<th class=' + JSON.stringify(cssClass) + '>' + v.type + '</th>'+
              '<th class=' + JSON.stringify(cssClass) + '>' + v.unique + '</th>'+
              '<th class=' + JSON.stringify(cssClass) + '>' + fieldString + '</th>'+
            '</tr>'
          );
        });
      }
    },

    fillLoadedModal: function () {
      $('#collectionSizeBox').show();
      $('#collectionSyncBox').show();
      $('#collectionRevBox').show();
      if (this.data.waitForSync === false) {
        $('#show-collection-sync').text('false');
      }
      else {
        $('#show-collection-sync').text('true');
      }
      var calculatedSize = this.data.journalSize / 1024 / 1024;
      $('#show-collection-size').text(this.roundNumber(calculatedSize, 2));
      $('#show-collection-rev').text(this.revision.revision);

      this.appendIndex();
//      this.appendFigures();

      $('#show-collection').modal('show');
    },
    getCollectionId: function () {
      return this.myCollection.id;
    },
    getCollectionStatus: function () {
      return this.myCollection.status;
    },
    hideModal: function () {
      $('#show-collection').modal('hide');
    }

  });
}());
