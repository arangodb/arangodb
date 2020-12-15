/* jshint browser: true */
/* jshint unused: false */
/* global frontendConfig, arangoHelper, prettyBytes, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  'use strict';

  window.ClusterView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('clusterView.ejs'),

    events: {
    },

    coordshortSuccess: undefined,
    statsEnabled: false,
    historyInit: false,
    initDone: false,
    interval: 10000,
    maxValues: 100,
    knownServers: [],
    chartData: {},
    charts: {},
    nvcharts: [],
    startHistory: {},
    startHistoryAccumulated: {},

    initialize: function (options) {
      var self = this;

      if (window.App.isCluster) {
        this.dbServers = options.dbServers;
        this.coordinators = options.coordinators;
        this.updateServerTime();

        // start polling with interval
        window.setInterval(function () {
          if (window.location.hash === '#cluster' ||
            window.location.hash === '' ||
            window.location.hash === '#') {
            var callback = function (data) {
              self.rerenderValues(data);
              self.rerenderGraphs(data);
            };

            // now fetch the statistics history
            self.getCoordStatHistory(callback);
          } else {
            var cb2 = function (data) {
              self.rerenderGraphs(data, true);
            };

            // only fetch data when view is not active
            self.getCoordStatHistory(cb2);
          }
        }, this.interval);
      }
    },

    render: function () {
      var self = this;
      this.$el.html(this.template.render({}));
      // this.initValues()

      if (!this.initDone) {
        if (this.coordinators.first() !== undefined) {
          this.getServerStatistics();
        } else {
          this.waitForCoordinators();
        }
        this.initDone = true;
      }
      this.initGraphs();

      // directly rerender coord, db, data
      var callback = function (data) {
        self.rerenderValues(data);
      };
      this.getCoordStatHistory(callback);
    },

    waitForCoordinators: function () {
      var self = this;

      window.setTimeout(function () {
        if (self.coordinators) {
          self.getServerStatistics();
        } else {
          self.waitForCoordinators();
        }
      }, 500);
    },

    updateServerTime: function () {
      this.serverTime = new Date().getTime();
    },

    getServerStatistics: function () {
      var self = this;

      this.data = undefined;

      var coord = this.coordinators.first();

      this.statCollectCoord = new window.ClusterStatisticsCollection([],
        {host: coord.get('address')}
      );
      this.statCollectDBS = new window.ClusterStatisticsCollection([],
        {host: coord.get('address')}
      );

      // create statistics collector for DB servers
      var dbsmodels = [];
      _.each(this.dbServers, function (dbs) {
        dbs.each(function (model) {
          dbsmodels.push(model);
        });
      });

      _.each(dbsmodels, function (dbserver) {
        if (dbserver.get('status') !== 'ok') {
          return;
        }

        if (self.knownServers.indexOf(dbserver.id) === -1) {
          self.knownServers.push(dbserver.id);
        }

        var stat = new window.Statistics({name: dbserver.id});
        stat.url = coord.get('protocol') + '://' +
        coord.get('address') +
        '/_admin/clusterStatistics?DBserver=' +
        dbserver.get('name');
        self.statCollectDBS.add(stat);
      });

      // create statistics collector for coordinator
      this.coordinators.forEach(function (coordinator) {
        if (coordinator.get('status') !== 'ok') {
          return;
        }

        if (self.knownServers.indexOf(coordinator.id) === -1) {
          self.knownServers.push(coordinator.id);
        }

        var stat = new window.Statistics({name: coordinator.id});

        stat.url = coordinator.get('protocol') + '://' +
          coordinator.get('address') +
          '/_admin/statistics';

        self.statCollectCoord.add(stat);
      });

      // first load history callback
      var callback = function (data) {
        self.rerenderValues(data);
        self.rerenderGraphs(data);
      };

      // now fetch the statistics history
      self.getCoordStatHistory(callback);

      // special case nodes
      self.renderNodes();
    },

    rerenderValues: function (data) {
      var self = this;

      // NODES - DBS - COORDS
      self.renderNodes();

      // Connections
      this.renderValue('#clusterConnections', Math.round(data.clientConnectionsCurrent));
      this.renderValue('#clusterConnectionsAvg', Math.round(data.clientConnections15M));

      // RAM
      var totalMem = data.physicalMemory;
      var usedMem = data.residentSizeCurrent;
      this.renderValue('#clusterRam', [usedMem, totalMem]);
    },

    renderValue: function (id, value, error, warning) {
      if (typeof value === 'number') {
        $(id).html(value);
      } else if ($.isArray(value)) {
        var a = value[0]; 
        var b = value[1];

        var percent = 1 / (b / a) * 100;
        if (percent > 90) {
          error = true;
        } else if (percent > 70 && percent < 90) {
          warning = true;
        }
        if (isNaN(percent)) {
          $(id).html('n/a');
        } else {
          $(id).html(percent.toFixed(1) + ' %');
        }
      } else if (typeof value === 'string') {
        $(id).html(value);
      }

      if (error) {
        $(id).addClass('negative');
        $(id).removeClass('warning');
        $(id).removeClass('positive');
      } else if (warning) {
        $(id).addClass('warning');
        $(id).removeClass('positive');
        $(id).removeClass('negative');
      } else {
        $(id).addClass('positive');
        $(id).removeClass('negative');
        $(id).removeClass('warning');
      }
    },

    renderNodes: function () {
      var self = this;
      var callbackFunction = function (data) {
        var coords = 0; var coordsErrors = 0;
        var dbs = 0; var dbsErrors = 0;

        _.each(data, function (node) {
          if (node.Role === 'Coordinator') {
            coords++;
            if (node.Status !== 'GOOD') {
              coordsErrors++;
            }
          } else if (node.Role === 'DBServer') {
            dbs++;
            if (node.Status !== 'GOOD') {
              dbsErrors++;
            }
          }
        });

        if (coordsErrors > 0) {
          this.renderValue('#clusterCoordinators', coords - coordsErrors + '/' + coords, true);
        } else {
          this.renderValue('#clusterCoordinators', coords);
        }

        if (dbsErrors > 0) {
          this.renderValue('#clusterDBServers', dbs - dbsErrors + '/' + dbs, true);
        } else {
          this.renderValue('#clusterDBServers', dbs);
        }
      }.bind(this);

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/health'),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: function (data) {
          callbackFunction(data.Health);
        },
        error: function () {
          self.renderValue('#clusterCoordinators', 'N/A', true);
          self.renderValue('#clusterDBServers', 'N/A', true);
        }
      });
    },

    initValues: function () {
      var values = [
        '#clusterNodes',
        '#clusterRam',
        '#clusterConnections',
        '#clusterConnectionsAvg'
      ];

      _.each(values, function (id) {
        $(id).html('<i class="fa fa-spin fa-circle-o-notch" style="color: rgba(0, 0, 0, 0.64);"></i>');
      });
    },

    graphData: {
      data: {
        sent: [],
        received: []
      },
      http: [],
      average: []
    },

    checkArraySizes: function () {
      var self = this;

      _.each(self.chartsOptions, function (val1, key1) {
        _.each(val1.options, function (val2, key2) {
          if (val2.values.length > self.maxValues - 1) {
            self.chartsOptions[key1].options[key2].values.shift();
          }
        });
      });
    },

    formatDataForGraph: function (data) {
      var self = this;

      if (!self.historyInit) {
        _.each(data.times, function (time, key) {
          // DATA
          self.chartsOptions[0].options[0].values.push({x: time, y: data.bytesSentPerSecond[key]});
          self.chartsOptions[0].options[1].values.push({x: time, y: data.bytesReceivedPerSecond[key]});

          // HTTP
          self.chartsOptions[1].options[0].values.push({x: time, y: self.calcTotalHttp(data.http, key)});

          // AVERAGE
          self.chartsOptions[2].options[0].values.push({x: time, y: data.avgRequestTime[key]});
        });
        self.historyInit = true;
      } else {
        self.checkArraySizes();

        // DATA
        self.chartsOptions[0].options[0].values.push({
          x: data.times[data.times.length - 1],
          y: data.bytesSentPerSecond[data.bytesSentPerSecond.length - 1]
        });
        self.chartsOptions[0].options[1].values.push({
          x: data.times[data.times.length - 1],
          y: data.bytesReceivedPerSecond[data.bytesReceivedPerSecond.length - 1]
        });
        // HTTP
        self.chartsOptions[1].options[0].values.push({
          x: data.times[data.times.length - 1],
          y: self.calcTotalHttp(data.http, data.bytesSentPerSecond.length - 1)
        });
        // AVERAGE
        self.chartsOptions[2].options[0].values.push({
          x: data.times[data.times.length - 1],
          y: data.avgRequestTime[data.bytesSentPerSecond.length - 1]
        });
      }
    },

    chartsOptions: [
      {
        id: '#clusterData',
        type: 'bytes',
        count: 2,
        options: [
          {
            area: true,
            values: [],
            key: 'Bytes out',
            color: 'rgb(23,190,207)',
            strokeWidth: 2,
            fillOpacity: 0.1
          },
          {
            area: true,
            values: [],
            key: 'Bytes in',
            color: 'rgb(188, 189, 34)',
            strokeWidth: 2,
            fillOpacity: 0.1
          }
        ]
      },
      {
        id: '#clusterHttp',
        type: 'number',
        options: [{
          area: true,
          values: [],
          key: 'Requests per second',
          color: 'rgb(0, 166, 90)',
          fillOpacity: 0.1
        }]
      },
      {
        id: '#clusterAverage',
        data: [],
        type: 'seconds',
        options: [{
          area: true,
          values: [],
          key: 'Seconds',
          color: 'rgb(243, 156, 18)',
          fillOpacity: 0.1
        }]
      }
    ],

    initGraphs: function () {
      var self = this;

      var noData = 'No data...';

      _.each(self.chartsOptions, function (c) {
        nv.addGraph(function () {
          self.charts[c.id] = nv.models.stackedAreaChart()
            .options({
              useInteractiveGuideline: true,
              showControls: false,
              noData: noData,
              duration: 0
            });

          self.charts[c.id].xAxis
            .axisLabel('')
            .tickFormat(function (d) {
              var x = new Date(d * 1000);
              return (x.getHours() < 10 ? '0' : '') + x.getHours() + ':' +
                (x.getMinutes() < 10 ? '0' : '') + x.getMinutes() + ':' +
                (x.getSeconds() < 10 ? '0' : '') + x.getSeconds();
            })
            .staggerLabels(false);

          self.charts[c.id].yAxis
            .axisLabel('')
            .tickFormat(function (d) {
              var formatted;

              if (c.type === 'bytes') {
                if (d === null) {
                  return 'N/A';
                }
                formatted = parseFloat(d3.format('.2f')(d));
                return prettyBytes(formatted);
              } else if (c.type === 'seconds') {
                if (d === null) {
                  return 'N/A';
                }
                formatted = parseFloat(d3.format('.3f')(d));
                return formatted;
              } else if (c.type === 'number') {
                if (d === null) {
                  return 'N/A';
                } else {
                  formatted = parseFloat(d3.format('.2f')(d));
                  return formatted;
                }
              }
            });

          var data; var lines = self.returnGraphOptions(c.id);
          if (lines.length > 0) {
            _.each(lines, function (val, key) {
              c.options[key].values = val;
            });
          } else {
            c.options[0].values = [];
          }
          data = c.options;

          self.chartData[c.id] = d3.select(c.id).append('svg')
            .datum(data)
            .transition().duration(300)
            .call(self.charts[c.id])
            .each('start', function () {
              window.setTimeout(function () {
                d3.selectAll(c.id + ' *').each(function () {
                  if (this.__transition__) {
                    this.__transition__.duration = 0;
                  }
                });
              }, 0);
            });

          nv.utils.windowResize(self.charts[c.id].update);
          self.nvcharts.push(self.charts[c.id]);

          return self.charts[c.id];
        });
      });
    },

    returnGraphOptions: function (id) {
      var arr = [];
      if (id === '#clusterData') {
        // arr =  [this.graphData.data.sent, this.graphData.data.received]
        arr = [
          this.chartsOptions[0].options[0].values,
          this.chartsOptions[0].options[1].values
        ];
      } else if (id === '#clusterHttp') {
        arr = [this.chartsOptions[1].options[0].values];
      } else if (id === '#clusterAverage') {
        arr = [this.chartsOptions[2].options[0].values];
      } else {
        arr = [];
      }

      return arr;
    },

    rerenderGraphs: function (input, noRender) {
      if (!this.statsEnabled) {
        return;
      }

      var self = this; var data; var lines;
      this.formatDataForGraph(input);

      _.each(self.chartsOptions, function (c) {
        lines = self.returnGraphOptions(c.id);

        if (lines.length > 0) {
          _.each(lines, function (val, key) {
            c.options[key].values = val;
          });
        } else {
          c.options[0].values = [];
        }
        data = c.options;

        if (noRender === undefined || noRender === false) {
          // update nvd3 chart
          if (data[0].values.length > 0) {
            if (self.historyInit) {
              if (self.charts[c.id]) {
                self.charts[c.id].update();
              }
            }
          }
        }
      });
    },

    calcTotalHttp: function (object, pos) {
      var sum = 0;
      _.each(object, function (totalHttp) {
        sum += totalHttp[pos];
      });
      return sum;
    },

    getCoordStatHistory: function (callback) {
      var self = this;

      if (this.coordshortSuccess || this.coordshortSuccess === undefined || (Date.now() - this.coordshortTimestamp) / 1000 > 60) {
        this.coordshortSuccess = false;
        var url = 'statistics/coordshort';

        if (frontendConfig.react) {
          url = arangoHelper.databaseUrl('/_admin/aardvark/' + url);
        }

        $.ajax({
          url: url,
          json: true,
          type: "GET",
          success: function (data) {
            self.coordshortTimestamp = Date.now();
            self.coordshortSuccess = true;
            self.statsEnabled = data.enabled;
            callback(data.data);
          },
          error: function () {
            self.coordshortTimestamp = Date.now();
            self.coordshortSuccess = false;
          }
        });
      }
    }

  });
}());
