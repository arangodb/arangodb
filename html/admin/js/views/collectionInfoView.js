/*jslint indent: 2, stupid: true, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, window, exports, Backbone, EJS, $, arangoHelper */

var collectionInfoView = Backbone.View.extend({
  el: '#modalPlaceholder',
  chart: null,

  initialize: function () {
  },

  template: new EJS({url: 'js/templates/collectionInfoView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    $('#show-collection').modal('show');
    $('#show-collection').on('hidden', function () {
    });
    $('#show-collection').on('shown', function () {
      $('#show-collection-name').focus();
    });
    this.fillModal();
    $('.modalInfoTooltips').tooltip({
      placement: "right"
    });

    return this;
  },
  events: {
    "hidden #show-collection"           :    "hidden",
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

    $('#show-collection-name').text(this.myCollection.name);
    $('#show-collection-id').text(this.myCollection.id);
    $('#show-collection-type').text(this.myCollection.type);
    $('#show-collection-status').text(this.myCollection.status);

    if (this.myCollection.status === 'unloaded') {
      $('#colFooter').append(
        '<div>For more information, collection has to be loaded</div>'
      );
      $('#collectionSizeBox').hide();
      $('#collectionSyncBox').hide();
    }
    else if (this.myCollection.status === 'loaded') {
      this.data = window.arangoCollectionsStore.getFigures(this.options.colId, true);
      this.fillLoadedModal(this.data);
      this.convertFigures(this.data);
      this.renderFigures();
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
  fillLoadedModal: function (data) {
    $('#collectionSizeBox').show();
    $('#collectionSyncBox').show();
    if (data.waitForSync === false) {
      $('#show-collection-sync').text('false');
    }
    else {
      $('#show-collection-sync').text('true');
    }
    var calculatedSize = data.journalSize / 1024 / 1024;
    $('#show-collection-size').text(calculatedSize);
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
