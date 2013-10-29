/*jslint indent: 2, stupid: true, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, window, exports, Backbone, EJS, $, arangoHelper, nv, d3*/

var collectionInfoView = Backbone.View.extend({
  el: '#modalPlaceholder',

  initialize: function () {
  },

  template: new EJS({url: 'js/templates/collectionInfoView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    $('#show-collection').modal('show');
    this.fillModal();

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
      this.fillLoadedModal(this.data);
    }
  },
  roundNumber: function(number, n) {
    var factor;
    factor = Math.pow(10,n);
    var returnVal = (Math.round(number * factor) / factor);
    return returnVal;
  },
  appendFigures: function () {
    var cssClass = 'modal-text';

    if (this.data) {
      $('#figures').append(
        '<table id="figures1">'+
          '<tr class="figuresHeader">'+
            '<th class="">Type</th>'+
            '<th>Count</th>'+
            '<th>Size (MB)</th>'+
            '<th>Info</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Datafiles</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.datafiles.count+'</th>'+
            '<th class="'+cssClass+'">'+
              this.roundNumber(this.data.figures.datafiles.fileSize / 1024 / 1024, 2)+
            '</th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="Number of active datafiles.">'+
              '<span class="glyphicon glyphicon-info-sign"></span></a>'+
            '</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Journals</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.journals.count+'</th>'+
            '<th class="'+cssClass+'">'+
              this.roundNumber(this.data.figures.journals.fileSize / 1024 / 1024, 2)+
            '</th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="Number of journal files.">'+
              '<span class="glyphicon glyphicon-info-sign"></span></a>'+
            '</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Compactors</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.compactors.count+'</th>'+
            '<th class="'+cssClass+'">'+
              this.roundNumber(this.data.figures.compactors.fileSize / 1024 / 1024, 2)+
            '</th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="Number of compactor files.">'+
              '<span class="glyphicon glyphicon-info-sign"></span></a>'+
            '</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Shape files</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.shapefiles.count+'</th>'+
            '<th class="'+cssClass+'">'+
              this.roundNumber(this.data.figures.shapefiles.fileSize / 1024 / 1024, 2)+
            '</th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="Number of shape files.">'+
              '<span class="glyphicon glyphicon-info-sign"></span></a>'+
            '</th>'+
          '</tr>'+

        '</table>'+

        '<table id="figures2">'+
          '<tr class="figuresHeader">'+
            '<th>Type</th>'+
            '<th>Count</th>'+
            '<th>Info</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Shapes</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.shapes.count+'</th>'+
              '<th class="tooltipInfoTh '+cssClass+'">'+
          '<a class="modalInfoTooltips" title="Total number of shapes used in the collection">'+
              '<span class="glyphicon glyphicon-info-sign"></span></a>'+
            '</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Attributes</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.attributes.count+'</th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="' +
              'Total number of attributes used in the collection">'+
              '<span class="glyphicon glyphicon-info-sign"></span></a>'+
            '</th>'+
          '</tr>'+
        '</table>'+

        '<table id="figures3">'+
          '<tr class="figuresHeader">'+
            '<th>Type</th>'+
            '<th>Count</th>'+
            '<th>Size (MB)</th>'+
            '<th>Deletion</th>'+
            '<th>Info</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Alive</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.alive.count+'</th>'+
            '<th class="'+cssClass+'">'+
              this.roundNumber(this.data.figures.alive.size/1024/1024, 2)+
            '</th>'+
            '<th class="'+cssClass+'"> - </th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="' + 
              'Total number and size used by all living documents.">'+
              '<span class="glyphicon glyphicon-info-sign"></span></a>'+
            '</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Dead</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.dead.count+'</th>'+
            '<th class="'+cssClass+'">'+
              this.roundNumber(this.data.figures.dead.size/1024/1024, 2)+
            '</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.dead.deletion+'</th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="' +
              'Total number and size used by all dead documents.">'+
              '<span class="glyphicon glyphicon-info-sign"></span></a>'+
            '</th>'+
          '</tr>'+
        '</table>'

      );
    }
  },

  appendIndex: function () {
    var cssClass = 'collectionInfoTh modal-text';
    if (this.index) {
      var fieldString = '';
      var indexId = '';
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

  fillLoadedModal: function (data) {
    $('#collectionSizeBox').show();
    $('#collectionSyncBox').show();
    $('#collectionRevBox').show();
    if (data.waitForSync === false) {
      $('#show-collection-sync').text('false');
    }
    else {
      $('#show-collection-sync').text('true');
    }
    var calculatedSize = data.journalSize / 1024 / 1024;
    $('#show-collection-size').text(this.roundNumber(calculatedSize, 2));
    $('#show-collection-rev').text(this.revision.revision);

    this.appendIndex();
    this.appendFigures();

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
