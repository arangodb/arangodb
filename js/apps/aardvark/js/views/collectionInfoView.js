/*jslint indent: 2, stupid: true, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, window, exports, Backbone, EJS, $, arangoHelper, nv, d3*/

var collectionInfoView = Backbone.View.extend({
  el: '#modalPlaceholder',
  chart: null,

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
    });

    return this;
  },
  events: {
    "hidden #show-collection" : "hidden"
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

    $('#show-collection-name').text("Collection: "+this.myCollection.name);
    $('#show-collection-id').text(this.myCollection.id);
    $('#show-collection-type').text(this.myCollection.type);
    $('#show-collection-status').text(this.myCollection.status);

    if (this.myCollection.status === 'loaded') {
      this.data = window.arangoCollectionsStore.getFigures(this.options.colId, true);
      this.revision = window.arangoCollectionsStore.getRevision(this.options.colId, true);
      this.properties = window.arangoCollectionsStore.getProperties(this.options.colId, true);
      this.index = window.arangoCollectionsStore.getIndex(this.options.colId, true);
      this.fillLoadedModal(this.data);
      //this.convertFigures(this.data);
      //this.renderFigures();
    }
  },
  renderFigures: function () {
    var self = this;

    // prevent some d3-internal races with a timeout
    window.setTimeout(function () {
      var chart = nv.models.pieChart()
      .x(function(d) { return d.label; })
      .y(function(d) { return d.value; })
      .showLabels(true);
      nv.addGraph(function() {
        d3.select(".modal-body-right svg")
        .datum(self.convertFigures())
        .transition().duration(1200)
        .call(chart);
        return chart;
      });

      return chart;
    }, 500);
  },
  convertFigures: function () {
    var self = this;
    var collValues = [];
    if (self.data && self.data.figures) {
      $.each(self.data.figures, function(k,v) {
        collValues.push({
          "label" : k,
          "value" : v.count
        });
      });
    }

    return [{
      key: "Collections Status",
      values: collValues
    }];
  },
  roundNumber: function(number, n) {
    var faktor;
    faktor = Math.pow(10,n);
    var returnVal = (Math.round(number * faktor) / faktor);
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
              this.roundNumber(this.data.figures.datafiles.fileSize / 1024 / 1024 , 2)+
            '</th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="Number of active datafiles.">'+
              '<span class="glyphicon glyphicon-info-sign" style="color:black"></span></a>'+
            '</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Journals</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.journals.count+'</th>'+
            '<th class="'+cssClass+'">'+
              this.roundNumber(this.data.figures.journals.fileSize / 1024 / 1024 , 2)+
            '</th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="Number of journal files.">'+
              '<span class="glyphicon glyphicon-info-sign" style="color:black"></span></a>'+
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
              '<span class="glyphicon glyphicon-info-sign" style="color:black"></span></a>'+
            '</th>'+
          '</tr>'+
          '<tr>'+
            '<th class="'+cssClass+'">Attributes</th>'+
            '<th class="'+cssClass+'">'+this.data.figures.attributes.count+'</th>'+
            '<th class="tooltipInfoTh '+cssClass+'">'+
              '<a class="modalInfoTooltips" title="' +
              'Total number of attributes used in the collection">'+
              '<span class="glyphicon glyphicon-info-sign" style="color:black"></span></a>'+
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
              '<span class="glyphicon glyphicon-info-sign" style="color:black"></span></a>'+
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
              '<span class="glyphicon glyphicon-info-sign" style="color:black"></span></a>'+
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
        fieldString = '';
        var counter = 1;

        //prettify json-array to string
        $.each(v.fields, function(k,v) {
          fieldString = fieldString + v;
          if (counter > 1) {
            fieldString = ', ' + fieldString;
          }
          counter++;
        });

        //cut index id
        var position = v.id.indexOf('/');
        var indexId = v.id.substr(position+1, v.id.length);

        $('#collectionIndexTable').append(
          '<tr>'+
            '<th class='+JSON.stringify(cssClass)+'>'+indexId+'</th>'+
            '<th class='+JSON.stringify(cssClass)+'>'+v.type+'</th>'+
            '<th class='+JSON.stringify(cssClass)+'>'+v.unique+'</th>'+
            '<th class='+JSON.stringify(cssClass)+'>'+fieldString+'</th>'+
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
    $('#show-collection-size').text(this.roundNumber(calculatedSize,2));
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
