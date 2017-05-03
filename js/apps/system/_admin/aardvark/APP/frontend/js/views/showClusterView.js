/* global window, $, Backbone, templateEngine,  _, d3, Dygraph, document */

(function () {
  'use strict';

  window.ShowClusterView = Backbone.View.extend({
    detailEl: '#modalPlaceholder',
    el: '#content',
    defaultFrame: 20 * 60 * 1000,
    template: templateEngine.createTemplate('showCluster.ejs'),
    modal: templateEngine.createTemplate('waitModal.ejs'),
    detailTemplate: templateEngine.createTemplate('detailView.ejs'),

    events: {
      'change #selectDB': 'updateCollections',
      'change #selectCol': 'updateShards',
      'click .dbserver.success': 'dashboard',
      'click .coordinator.success': 'dashboard'
    },

    replaceSVGs: function () {
      $('.svgToReplace').each(function () {
        var img = $(this);
        var id = img.attr('id');
        var src = img.attr('src');
        $.get(src, function (d) {
          var svg = $(d).find('svg');
          svg.attr('id', id)
            .attr('class', 'icon')
            .removeAttr('xmlns:a');
          img.replaceWith(svg);
        }, 'xml');
      });
    },

    updateServerTime: function () {
      this.serverTime = new Date().getTime();
    },

    setShowAll: function () {
      this.graphShowAll = true;
    },

    resetShowAll: function () {
      this.graphShowAll = false;
      this.renderLineChart();
    },

    initialize: function (options) {
      this.options = options;
      this.interval = 10000;
      this.isUpdating = false;
      this.timer = null;
      this.knownServers = [];
      this.graph = undefined;
      this.graphShowAll = false;
      this.updateServerTime();
      this.dygraphConfig = this.options.dygraphConfig;
      this.dbservers = new window.ClusterServers([], {
        interval: this.interval
      });
      this.coordinators = new window.ClusterCoordinators([], {
        interval: this.interval
      });
      this.documentStore = new window.ArangoDocuments();
      this.statisticsDescription = new window.StatisticsDescription();
      this.statisticsDescription.fetch({
        async: false
      });
      this.dbs = new window.ClusterDatabases([], {
        interval: this.interval
      });
      this.cols = new window.ClusterCollections();
      this.shards = new window.ClusterShards();
      this.startUpdating();
    },

    listByAddress: function (callback) {
      var byAddress = {};
      var self = this;
      this.dbservers.byAddress(byAddress, function (res) {
        self.coordinators.byAddress(res, callback);
      });
    },

    updateCollections: function () {
      var self = this;
      var selCol = $('#selectCol');
      var dbName = $('#selectDB').find(':selected').attr('id');
      if (!dbName) {
        return;
      }
      var colName = selCol.find(':selected').attr('id');
      selCol.html('');
      this.cols.getList(dbName, function (list) {
        _.each(_.pluck(list, 'name'), function (c) {
          selCol.append('<option id="' + c + '">' + c + '</option>');
        });
        var colToSel = $('#' + colName, selCol);
        if (colToSel.length === 1) {
          colToSel.prop('selected', true);
        }
        self.updateShards();
      });
    },

    updateShards: function () {
      var dbName = $('#selectDB').find(':selected').attr('id');
      var colName = $('#selectCol').find(':selected').attr('id');
      this.shards.getList(dbName, colName, function (list) {
        $('.shardCounter').html('0');
        _.each(list, function (s) {
          $('#' + s.server + 'Shards').html(s.shards.length);
        });
      });
    },

    updateServerStatus: function (nextStep) {
      var self = this;
      var callBack = function (cls, stat, serv) {
        var id = serv;
        var type;
        var icon;
        id = id.replace(/\./g, '-');
        id = id.replace(/:/g, '_');
        icon = $('#id' + id);
        if (icon.length < 1) {
          // callback after view was unrendered
          return;
        }
        type = icon.attr('class').split(/\s+/)[1];
        icon.attr('class', cls + ' ' + type + ' ' + stat);
        if (cls === 'coordinator') {
          if (stat === 'success') {
            $('.button-gui', icon.closest('.tile')).toggleClass('button-gui-disabled', false);
          } else {
            $('.button-gui', icon.closest('.tile')).toggleClass('button-gui-disabled', true);
          }
        }
      };
      this.coordinators.getStatuses(callBack.bind(this, 'coordinator'), function () {
        self.dbservers.getStatuses(callBack.bind(self, 'dbserver'));
        nextStep();
      });
    },

    updateDBDetailList: function () {
      var self = this;
      var selDB = $('#selectDB');
      var dbName = selDB.find(':selected').attr('id');
      selDB.html('');
      this.dbs.getList(function (dbList) {
        _.each(_.pluck(dbList, 'name'), function (c) {
          selDB.append('<option id="' + c + '">' + c + '</option>');
        });
        var dbToSel = $('#' + dbName, selDB);
        if (dbToSel.length === 1) {
          dbToSel.prop('selected', true);
        }
        self.updateCollections();
      });
    },

    rerender: function () {
      var self = this;
      this.updateServerStatus(function () {
        self.getServerStatistics(function () {
          self.updateServerTime();
          self.data = self.generatePieData();
          self.renderPieChart(self.data);
          self.renderLineChart();
          self.updateDBDetailList();
        });
      });
    },

    render: function () {
      this.knownServers = [];
      delete this.hist;
      var self = this;
      this.listByAddress(function (byAddress) {
        if (Object.keys(byAddress).length === 1) {
          self.type = 'testPlan';
        } else {
          self.type = 'other';
        }
        self.updateDBDetailList();
        self.dbs.getList(function (dbList) {
          $(self.el).html(self.template.render({
            dbs: _.pluck(dbList, 'name'),
            byAddress: byAddress,
            type: self.type
          }));
          $(self.el).append(self.modal.render({}));
          self.replaceSVGs();
          /* this.loadHistory(); */
          self.getServerStatistics(function () {
            self.data = self.generatePieData();
            self.renderPieChart(self.data);
            self.renderLineChart();
            self.updateDBDetailList();
            self.startUpdating();
          });
        });
      });
    },

    generatePieData: function () {
      var pieData = [];
      var self = this;

      this.data.forEach(function (m) {
        pieData.push({
          key: m.get('name'),
          value: m.get('system').virtualSize,
          time: self.serverTime
        });
      });

      return pieData;
    },

    /*
     loadHistory : function() {
       this.hist = {}

       var self = this
       var coord = this.coordinators.findWhere({
         status: "ok"
       })

       var endpoint = coord.get("protocol")
       + "://"
       + coord.get("address")

       this.dbservers.forEach(function (dbserver) {
         if (dbserver.get("status") !== "ok") {return;}

         if (self.knownServers.indexOf(dbserver.id) === -1) {
           self.knownServers.push(dbserver.id)
         }

         var server = {
           raw: dbserver.get("address"),
           isDBServer: true,
           target: encodeURIComponent(dbserver.get("name")),
           endpoint: endpoint,
           addAuth: window.App.addAuth.bind(window.App)
         }
       })

       this.coordinators.forEach(function (coordinator) {
         if (coordinator.get("status") !== "ok") {return;}

         if (self.knownServers.indexOf(coordinator.id) === -1) {
           self.knownServers.push(coordinator.id)
         }

         var server = {
           raw: coordinator.get("address"),
           isDBServer: false,
           target: encodeURIComponent(coordinator.get("name")),
           endpoint: coordinator.get("protocol") + "://" + coordinator.get("address"),
           addAuth: window.App.addAuth.bind(window.App)
         }
       })
     },
     */

    addStatisticsItem: function (name, time, requests, snap) {
      var self = this;

      if (!self.hasOwnProperty('hist')) {
        self.hist = {};
      }

      if (!self.hist.hasOwnProperty(name)) {
        self.hist[name] = [];
      }

      var h = self.hist[name];
      var l = h.length;

      if (l === 0) {
        h.push({
          time: time,
          snap: snap,
          requests: requests,
          requestsPerSecond: 0
        });
      } else {
        var lt = h[l - 1].time;
        var tt = h[l - 1].requests;

        if (tt < requests) {
          var dt = time - lt;
          var ps = 0;

          if (dt > 0) {
            ps = (requests - tt) / dt;
          }

          h.push({
            time: time,
            snap: snap,
            requests: requests,
            requestsPerSecond: ps
          });
        }
      }
    },

    getServerStatistics: function (nextStep) {
      var self = this;
      var snap = Math.round(self.serverTime / 1000);

      this.data = undefined;

      var statCollect = new window.ClusterStatisticsCollection();
      var coord = this.coordinators.first();

      // create statistics collector for DB servers
      this.dbservers.forEach(function (dbserver) {
        if (dbserver.get('status') !== 'ok') { return; }

        if (self.knownServers.indexOf(dbserver.id) === -1) {
          self.knownServers.push(dbserver.id);
        }

        var stat = new window.Statistics({name: dbserver.id});

        stat.url = coord.get('protocol') + '://' +
        coord.get('address') +
        '/_admin/clusterStatistics?DBserver=' +
        dbserver.get('name');

        statCollect.add(stat);
      });

      // create statistics collector for coordinator
      this.coordinators.forEach(function (coordinator) {
        if (coordinator.get('status') !== 'ok') { return; }

        if (self.knownServers.indexOf(coordinator.id) === -1) {
          self.knownServers.push(coordinator.id);
        }

        var stat = new window.Statistics({name: coordinator.id});

        stat.url = coordinator.get('protocol') + '://' +
          coordinator.get('address') +
          '/_admin/statistics';

        statCollect.add(stat);
      });

      var cbCounter = statCollect.size();

      this.data = [];

      var successCB = function (m) {
        cbCounter--;
        var time = m.get('time');
        var name = m.get('name');
        var requests = m.get('http').requestsTotal;

        self.addStatisticsItem(name, time, requests, snap);
        self.data.push(m);
        if (cbCounter === 0) {
          nextStep();
        }
      };
      var errCB = function () {
        cbCounter--;
        if (cbCounter === 0) {
          nextStep();
        }
      };
      // now fetch the statistics
      statCollect.fetch(successCB, errCB);
    },

    renderPieChart: function (dataset) {
      var w = $('#clusterGraphs svg').width();
      var h = $('#clusterGraphs svg').height();
      var radius = Math.min(w, h) / 2; // change 2 to 1.4. It's hilarious.
      // var color = d3.scale.category20()
      var color = this.dygraphConfig.colors;

      var arc = d3.svg.arc() // each datapoint will create one later.
        .outerRadius(radius - 20)
        .innerRadius(0);
      var pie = d3.layout.pie()
        .sort(function (d) {
          return d.value;
        })
        .value(function (d) {
          return d.value;
        });
      d3.select('#clusterGraphs').select('svg').remove();
      var pieChartSvg = d3.select('#clusterGraphs').append('svg')
        // .attr("width", w)
        // .attr("height", h)
        .attr('class', 'clusterChart')
        .append('g') // someone to transform. Groups data.
        .attr('transform', 'translate(' + w / 2 + ',' + ((h / 2) - 10) + ')');

      var arc2 = d3.svg.arc()
        .outerRadius(radius - 2)
        .innerRadius(radius - 2);
      var slices = pieChartSvg.selectAll('.arc')
        .data(pie(dataset))
        .enter().append('g')
        .attr('class', 'slice');
      slices.append('path')
        .attr('d', arc)
        .style('fill', function (item, i) {
          return color[i % color.length];
        })
        .style('stroke', function (item, i) {
          return color[i % color.length];
        });
      slices.append('text')
        .attr('transform', function (d) { return 'translate(' + arc.centroid(d) + ')'; })
        // .attr("dy", "0.35em")
        .style('text-anchor', 'middle')
        .text(function (d) {
          var v = d.data.value / 1024 / 1024 / 1024;
          return v.toFixed(2);
        });

      slices.append('text')
        .attr('transform', function (d) { return 'translate(' + arc2.centroid(d) + ')'; })
        // .attr("dy", "1em")
        .style('text-anchor', 'middle')
        .text(function (d) { return d.data.key; });
    },

    renderLineChart: function () {
      var self = this;

      var interval = 60 * 20;
      var data = [];
      var hash = [];
      var t = Math.round(new Date().getTime() / 1000) - interval;
      var ks = self.knownServers;
      var f = function () {
        return null;
      };

      var d, h, i, j, tt, snap;

      for (i = 0; i < ks.length; ++i) {
        h = self.hist[ks[i]];

        if (h) {
          for (j = 0; j < h.length; ++j) {
            snap = h[j].snap;

            if (snap < t) {
              continue;
            }

            if (!hash.hasOwnProperty(snap)) {
              tt = new Date(snap * 1000);

              d = hash[snap] = [ tt ].concat(ks.map(f));
            } else {
              d = hash[snap];
            }

            d[i + 1] = h[j].requestsPerSecond;
          }
        }
      }

      data = [];

      Object.keys(hash).sort().forEach(function (m) {
        data.push(hash[m]);
      });

      var options = this.dygraphConfig.getDefaultConfig('clusterRequestsPerSecond');
      options.labelsDiv = $('#lineGraphLegend')[0];
      options.labels = [ 'datetime' ].concat(ks);

      self.graph = new Dygraph(
        document.getElementById('lineGraph'),
        data,
        options
      );
    },

    stopUpdating: function () {
      window.clearTimeout(this.timer);
      delete this.graph;
      this.isUpdating = false;
    },

    startUpdating: function () {
      if (this.isUpdating) {
        return;
      }

      this.isUpdating = true;

      var self = this;

      this.timer = window.setInterval(function () {
        self.rerender();
      }, this.interval);
    },

    dashboard: function (e) {
      this.stopUpdating();

      var tar = $(e.currentTarget);
      var serv = {};
      var cur;
      var coord;

      var ipPort = tar.attr('id');
      ipPort = ipPort.replace(/-/g, '.');
      ipPort = ipPort.replace(/_/g, ':');
      ipPort = ipPort.substr(2);

      serv.raw = ipPort;
      serv.isDBServer = tar.hasClass('dbserver');

      if (serv.isDBServer) {
        cur = this.dbservers.findWhere({
          address: serv.raw
        });
        coord = this.coordinators.findWhere({
          status: 'ok'
        });
        serv.endpoint = coord.get('protocol') +
          '://' +
          coord.get('address');
      } else {
        cur = this.coordinators.findWhere({
          address: serv.raw
        });
        serv.endpoint = cur.get('protocol') +
          '://' +
          cur.get('address');
      }

      serv.target = encodeURIComponent(cur.get('name'));
      window.App.serverToShow = serv;
      window.App.dashboard();
    },

    getCurrentSize: function (div) {
      if (div.substr(0, 1) !== '#') {
        div = '#' + div;
      }
      var height, width;
      $(div).attr('style', '');
      height = $(div).height();
      width = $(div).width();
      return {
        height: height,
        width: width
      };
    },

    resize: function () {
      var dimensions;
      if (this.graph) {
        dimensions = this.getCurrentSize(this.graph.maindiv_.id);
        this.graph.resize(dimensions.width, dimensions.height);
      }
    }
  });
}());
