var dashboardView = Backbone.View.extend({
  el: '#content',

  collectionStats: {
    totalCollections: 0,
    loadedCollections: 0,
    unloadedCollections: 0,
    systemCollections: 0,
    deletedCollections: 0,
    newbornCollections: 0
  },

  init: function () {
  },

  template: new EJS({url: '/_admin/html/js/templates/dashboardView.ejs'}),

  render: function() {
    $.gritter.removeAll();
    $(this.el).html(this.template.text);
    this.updateCollectionsStats();
    this.renderCollections();
    return this;
  },
  renderCollections: function () {
    var self = this;
    nv.addGraph(function() {
      var chart = nv.models.discreteBarChart()
      .x(function(d) { return d.label })
      .y(function(d) { return d.value })
      .staggerLabels(false)
      .tooltips(true)
      .showValues(false);

      d3.select("#dashboardCollectionsGraph svg")
      .datum(self.formatCollectionsStats())
      .transition().duration(1200)
      .call(chart)

      return chart;
    });

    $('#dashboardCollectionsText').html('');
    $('#dashboardCollectionsText').append(''
      +'<table>'
        +'<tr>'
          +'<th>Total: </th>'
          +'<th>'+this.collectionStats.totalCollections+'</th>'
        +'</tr>'
        +'<tr>'
          +'<th>Loaded: </th>'
          +'<th>'+this.collectionStats.loadedCollections+'</th>'
        +'</tr>'
        +'<tr>'
          +'<th>Unloaded: </th>'
          +'<th>'+this.collectionStats.unloadedCollections+'</th>'
        +'</tr>'
        +'<tr>'
          +'<th>System: </th>'
          +'<th>'+this.collectionStats.systemCollections+'</th>'
        +'</tr>'
      +'</table>'
    );

  },
  updateCollectionsStats: function () {
    var self = this;
    this.collectionStats.loadedCollections = 0;
    this.collectionStats.unloadedCollections = 0;
    this.collectionStats.deletedCollections = 0;
    this.collectionStats.newbornCollections = 0;
    this.collectionStats.totalCollections = this.collection.length;

    this.collection.each(function (arango_collection) {
      if (arango_collection.get('status') === 'new born collection') {
        self.collectionStats.newbornCollections++;
      }
      else if (arango_collection.get('status') === 'loaded') {
        self.collectionStats.loadedCollections++;
      }
      else if (arango_collection.get('status') === 'unloaded') {
        self.collectionStats.unloadedCollections++;
      }
      else if (arango_collection.get('status') === 'deleted') {
        self.collectionStats.deletedCollections++;
      }
      if (arango_collection.get('name').substr(0,1) === "_") {
        self.collectionStats.systemCollections++;
      }
    });
  },
  formatCollectionsStats: function () {
    return [{
      key: "Collection Status",
      values: [
        {
          "label" : "total",
          "value" : this.collectionStats.totalCollections
        },
        {
          "label" : "loaded",
          "value" : this.collectionStats.loadedCollections
        },
        {
          "label" : "unloaded",
          "value" : this.collectionStats.unloadedCollections
        },
        {
          "label" : "system",
          "value" : this.collectionStats.systemCollections
        },
        {
          "label" : "new born",
          "value" : this.collectionStats.newbornCollections
        },
        {
          "label" : "deleted",
          "value" : this.collectionStats.deletedCollections
        }
      ]
    }]
  },

  updateSystem: function () {

  },

  updateClient: function () {

  }

});
