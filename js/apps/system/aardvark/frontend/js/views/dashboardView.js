/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, console, Dygraph, _,templateEngine */

(function () {
  "use strict";

  function fmtNumber (n, nk) {
    return n.toFixed(nk);
  }

  window.DashboardView = Backbone.View.extend({
    el: '#content',
    interval: 10000, // in milliseconds
    defaultFrame: 20 * 60 * 1000,
    defaultDetailFrame: 2 * 24 * 60 * 60 * 1000,
    history: {},
    graphs: {},
    alreadyCalledDetailChart: [],

    events: {
      "click .dashboard-chart": "showDetail"
    },

    tendencies: {
      asyncPerSecondCurrent: [
        "asyncPerSecondCurrent", "asyncPerSecondCurrentPercentChange"
      ],
      syncPerSecondCurrent: [
        "syncPerSecondCurrent", "syncPerSecondCurrentPercentChange"
      ],
      clientConnectionsCurrent: [
        "clientConnectionsCurrent", "clientConnectionsCurrentPercentChange"
      ],
      clientConnectionsAverage: [
        "clientConnections15M", "clientConnectionsPercentChange15M"
      ],
      numberOfThreadsCurrent: [
        "numberOfThreadsCurrent", "numberOfThreadsCurrentPercentChange"
      ],
      numberOfThreadsAverage: [
        "numberOfThreads15M", "numberOfThreadsPercentChange15M"
      ],
      virtualSizeCurrent: [
        "virtualSizeCurrent", "virtualSizePercentChange"
      ],
      virtualSizeAverage: [
        "virtualSize15M", "virtualSizePercentChange15M"
      ]
    },

    barCharts: {
      totalTimeDistribution: [
        "queueTimeDistributionPercent", "requestTimeDistributionPercent"
      ],
      dataTransferDistribution: [
        "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"
      ]
    },

    barChartsElementNames: {
      queueTimeDistributionPercent: "Queue",
      requestTimeDistributionPercent: "Computation",
      bytesSentDistributionPercent: "Bytes sent",
      bytesReceivedDistributionPercent: "Bytes received"

    },


    getDetailFigure : function (e) {
      var figure = $(e.currentTarget).attr("id").replace(/ChartContainer/g, "");
      figure = figure.replace(/DistributionContainer/g, "");
      figure = figure.replace(/Container/g, "");
      if (figure === "asyncRequests") {
        figure = "requestsAsync";
      } else if (figure === "clientConnections") {
        figure = "httpConnections";
      }
      return figure;
    },

    showDetail: function (e) {
      var self = this,
          figure = this.getDetailFigure(e), options;
      this.getStatistics(figure);
      this.detailGraphFigure = figure;
      options = this.dygraphConfig.getDetailChartConfig(figure);
      window.modalView.hideFooter = true;
      window.modalView.hide();
      window.modalView.show(
        "modalGraph.ejs",
        options.header,
        undefined,
        undefined,
        undefined,
        this.events

      );

      options = this.dygraphConfig.getDetailChartConfig(figure);
      window.modalView.hideFooter = false;

      $('#modal-dialog').on('hidden', function () {
        self.hidden();
      });
      $('#modal-dialog').toggleClass("modal-chart-detail", true);
      options.height = $('.modal-chart-detail').height() * 0.7;
      options.width = $('.modal-chart-detail').width() * 0.84;
      this.detailGraph = new Dygraph(
        document.getElementById("lineChartDetail"),
        this.history[figure],
        options
      );
    },

    hidden: function () {
      this.detailGraph.destroy();
      delete this.detailGraph;
      delete this.detailGraphFigure;
    },


    getCurrentSize: function (div) {
      if (div.substr(0,1) !== "#") {
        div = "#" + div;
      }
      var height, width;
      $(div).attr("style", "");
      height = $(div).height();
      width = $(div).width();
      return {
        height: height,
        width: width
      };
    },

    prepareDygraphs: function () {
      var self = this, options;
      this.dygraphConfig.getDashBoardFigures().forEach(function (f) {
        options = self.dygraphConfig.getDefaultConfig(f);
        var dimensions = self.getCurrentSize(options.div);
        options.height = dimensions.height;
        options.width = dimensions.width;
        self.graphs[f] = new Dygraph(
          document.getElementById(options.div),
          self.history[f] || [],
          options
        );
      });
    },

    initialize: function () {
      this.dygraphConfig = this.options.dygraphConfig;
      this.server = this.options.serverToShow;
      this.d3NotInitialised = true;
      this.events["click .dashboard-chart"] = this.showDetail.bind(this);
      this.events["mousedown .dygraph-rangesel-zoomhandle"] = this.stopUpdating.bind(this);
      this.events["mouseup .dygraph-rangesel-zoomhandle"] = this.startUpdating.bind(this);
    },

    updateCharts: function () {
      var self = this;
      if (this.detailGraph) {
        this.updateLineChart(this.detailGraphFigure, true);
        return;
      }
      this.prepareD3Charts(this.isUpdating);
      this.prepareResidentSize(this.isUpdating);
      this.updateTendencies();
      Object.keys(this.graphs).forEach(function (f) {
        self.updateLineChart(f, false);
      });
    },

    updateTendencies: function () {
      var self = this, map = this.tendencies;
			
			var lineBreak = "<br>";
			if ($(".dashboard-tendency-chart").width() < 298) {
      	lineBreak = "<br>"
      }
			
			var tempColor = "";
			
			Object.keys(map).forEach(function (a) {
				if (self.history[a][1] < 0) {
					tempColor = "red";
				}
				else {
					tempColor = "green";
				}
        $("#" + a).html(self.history[a][0] + lineBreak + '<div class="dashboard-figurePer" style="color: '+ tempColor +';">(' + self.history[a][1] + ' %)</div>');
      });
    },


    updateDateWindow: function (graph, isDetailChart) {
      var t = new Date().getTime();
      var borderLeft, borderRight;
      if (isDetailChart && graph.dateWindow_) {
        borderLeft = graph.dateWindow_[0];
        borderRight = t - graph.dateWindow_[1] - this.interval * 5 > 0 ?
        graph.dateWindow_[1] : t;
        return [borderLeft, borderRight];
      }
      return [t - this.defaultFrame, t];


    },

    updateLineChart: function (figure, isDetailChart) {
      var g = isDetailChart ? this.detailGraph : this.graphs[figure],
      opts = {
        file: this.history[figure],
        dateWindow: this.updateDateWindow(g, isDetailChart)
      };
      g.updateOptions(opts);
    },

    mergeDygraphHistory: function (newData, i) {
      var self = this, valueList;
      this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {
        if (!self.dygraphConfig.mapStatToFigure[f]) {
          return;
        }
        if (!self.history[f]) {
          self.history[f] = [];
        }
        valueList = [];
        self.dygraphConfig.mapStatToFigure[f].forEach(function (a) {
          if (!newData[a]) {
            return;
          }
          if (a === "times") {
            valueList.push(new Date(newData[a][i] * 1000));
          } else {
            valueList.push(newData[a][i]);
          }
        });
        if (valueList.length > 1) {
          self.history[f].push(valueList);
        }
      });
    },

    mergeHistory: function (newData, detailMode) {
      var self = this, i;
      for (i = 0; i < newData.times.length; ++i) {
        this.mergeDygraphHistory(newData, i);
      }
      if (detailMode) {
        return;
      }
      Object.keys(this.tendencies).forEach(function (a) {
        var n1 = 1;
        var n2 = 1;

        if (a === "virtualSizeCurrent" || a === "virtualSizeAverage") {
          newData[self.tendencies[a][0]] /= (1024 * 1024 * 1024);
          n1 = 2;
        }
        else if (a === "clientConnectionsCurrent") {
          n1 = 0;
        }
        else if (a === "numberOfThreadsCurrent") {
          n1 = 0;
        }

        self.history[a] = [
          fmtNumber(newData[self.tendencies[a][0]], n1),
          fmtNumber(newData[self.tendencies[a][1]] * 100, n2)
        ];
      });

      Object.keys(this.barCharts).forEach(function (a) {
        self.history[a] = self.mergeBarChartData(self.barCharts[a], newData);
      });
      self.history.physicalMemory = newData.physicalMemory;
      self.history.residentSizeChart =
      [
        {
          "key": "",
          "color": this.dygraphConfig.colors[1],
          "values": [
            {
              label: "used",
              value: newData.residentSizePercent[
                newData.residentSizePercent.length - 1] * 100
            }
          ]
        },
        {
          "key": "",
          "color": this.dygraphConfig.colors[0],
          "values": [
            {
              label: "used",
              value: 100 - newData.residentSizePercent[
                newData.residentSizePercent.length - 1] * 100
            }
          ]
        }
      ]
      ;


      this.nextStart = newData.nextStart;
    },

    mergeBarChartData: function (attribList, newData) {
      var i, v1 = {
        "key": this.barChartsElementNames[attribList[0]],
        "color": this.dygraphConfig.colors[0],
        "values": []
      }, v2 = {
        "key": this.barChartsElementNames[attribList[1]],
        "color": this.dygraphConfig.colors[1],
        "values": []
      };
      for (i = 0; i < newData[attribList[0]].values.length; ++i) {
        v1.values.push({
          label: this.getLabel(newData[attribList[0]].cuts, i),
          value: newData[attribList[0]].values[i]
        });
        v2.values.push({
          label: this.getLabel(newData[attribList[1]].cuts, i),
          value: newData[attribList[1]].values[i]
        });
      }
      return [v1, v2];
    },

    getLabel: function (cuts, counter) {
      if (!cuts[counter]) {
        return ">" + cuts[counter - 1];
      }
      return counter === 0 ? "0 - " +
                         cuts[counter] : cuts[counter - 1] + " - " + cuts[counter];
    },

    getStatistics: function (figure) {
      var self = this;
      var url = "statistics/full";
      var urlParams = "?start=";

      if (! figure && this.nextStart) {
        urlParams += this.nextStart;
      } else if (! figure && ! this.nextStart) {
        urlParams += (new Date().getTime() - this.defaultFrame) / 1000;
      } else {
        if (this.alreadyCalledDetailChart.indexOf(figure) !== -1) {
          return;
        }

        this.history[figure] = [];

        urlParams += (new Date().getTime() - this.defaultDetailFrame) / 1000;
        urlParams += "&filter=" + this.dygraphConfig.mapStatToFigure[figure].join();

        this.alreadyCalledDetailChart.push(figure);
      }

      if (this.server) {
        url = this.server.endpoint + "/_admin/aardvark/statistics/cluster";
        urlParams +=  "&DBserver=" + this.server.target;
      }

      $.ajax(
        url + urlParams,
        {async: false}
      ).done(
        function (d) {
          if (d.times.length > 0) {
            self.isUpdating = true;
            self.mergeHistory(d, !!figure);
          } else if (self.isUpdating !== true)  {
            window.modalView.show(
              "modalWarning.ejs",
              "WARNING !"
            );
            self.isUpdating = false;
          }
        }
      );
    },

    prepareResidentSize: function (update) {
      var self = this;

      var dimensions = this.getCurrentSize('#residentSizeChartContainer');

      var current = fmtNumber((self.history.physicalMemory * 100  / 1024 / 1024 / 100) * (self.history.residentSizeChart[0].values[0].value / 100), 1);
			if (current < 1025) {
				var currentA = current + " MB"
			}
			else {
				var currentA = fmtNumber((current / 1024), 1) + " GB"
				
			}
      var currentP = Math.round(self.history.residentSizeChart[0].values[0].value * 100) / 100;

      var data = [fmtNumber(self.history.physicalMemory * 100  / 1024 / 1024 / 1024, 1) / 100 + " GB"];

			nv.addGraph(function () {
        var chart = nv.models.multiBarHorizontalChart()
	        .x(function (d) {
	          return d.label;
	        })
	        .y(function (d) {
	          return d.value;
	        })
	        .width(dimensions.width)
	        .height(dimensions.height)
	        .margin({
	          top: ($(residentSizeChartContainer).outerHeight() - $(residentSizeChartContainer).height()) / 2,
	          right: 0,
	          bottom: ($(residentSizeChartContainer).outerHeight() - $(residentSizeChartContainer).height()) / 2,
	          left: 0
	        })
	        .showValues(false)
	        .showYAxis(false)
	        .showXAxis(false)
	        .transitionDuration(100)
	        .tooltips(false)
	        .showLegend(false)
	        .showControls(false)
	        .stacked(true);
        
        chart.yAxis
          .tickFormat(function (d) {return d + "%";})
          .showMaxMin(false);
				chart.xAxis.showMaxMin(false);
        
        d3.select('#residentSizeChart svg')
	        .datum(self.history.residentSizeChart)
	        .call(chart);
        
				d3.select('#residentSizeChart svg').select('.nv-zeroLine').remove();
				
        if (update) {
          d3.select('#residentSizeChart svg').select('#total').remove();
          d3.select('#residentSizeChart svg').select('#percentage').remove();
        }
        
        d3.select('.dashboard-bar-chart-title .percentage')
	        .html(currentA + " ("+ currentP + " %)");

        d3.select('.dashboard-bar-chart-title .absolut')
					.html(data[0]);
				
        nv.utils.windowResize(chart.update);
				
				return chart;
      }, function() {
        d3.selectAll("#residentSizeChart .nv-bar").on('click',
          function() {
            // no idea why this has to be empty, well anyways...
          }
        );
      });
    },


    prepareD3Charts: function (update) {
      var v, self = this, f;

      var barCharts = {
        totalTimeDistribution: [
          "queueTimeDistributionPercent", "requestTimeDistributionPercent"],
        dataTransferDistribution: [
          "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"]
      };

      var dists = {
        totalTimeDistribution: "Time Distribution",
        dataTransferDistribution: "Size Distribution"
      };

      if (this.d3NotInitialised) {
          update = false;
          this.d3NotInitialised = false;
      }

      _.each(Object.keys(barCharts), function (k) {
        var dimensions = self.getCurrentSize('#' + k
          + 'Container .dashboard-interior-chart');

        if (dimensions.width > 400 ) {
          f = 18;
        } else if (dimensions.width > 300) {
          f = 16;
        } else if (dimensions.width > 200) {
          f = 14;
        } else if (dimensions.width > 100) {
          f = 12;
        } else {
          f = 10;
        }

        var selector = "#" + k + "Container svg";
        var dist = dists[k];

        nv.addGraph(function () {
          var chart = nv.models.multiBarHorizontalChart()
          .x(function (d) {
            return d.label;
          })
          .y(function (d) {
            return d.value;
          })
          .width(dimensions.width)
          .height(dimensions.height)
          .margin({
            top: dimensions.height / 8,
            right: dimensions.width / 35,
            bottom: dimensions.height / 22,
            left: dimensions.width / 6
          })
          .showValues(false)
          .showYAxis(true)
          .showXAxis(true)
          .transitionDuration(350)
          .tooltips(false)
          .showLegend(false)
          .showControls(false);

          chart.yAxis
          .tickFormat(function (d) {return Math.round(d* 100 * 100) / 100 + "%";});


          d3.select(selector)
          .datum(self.history[k])
          .call(chart);

          nv.utils.windowResize(chart.update);

          if (!update) {
            var v1 = self.history[k][0].key;
            var v2 = self.history[k][1].key;
            $('#' + k + "Legend").append(
              '<span style="color: ' +
              self.history[k][0].color + ';">' +
              '<div style="display: inline-block; position: relative;' +
              ' bottom: .5ex; padding-left: 1em;' +
              ' height: 1px; border-bottom: 2px solid ' +
              self.history[k][0].color + ';"></div>'
              + " " + v1 + '</span><br>' +
              '<span style="color: ' +
              self.history[k][1].color + ';">' +
              '<div style="display: inline-block; position: ' +
              'relative; bottom: .5ex; padding-left: 1em;' +
              ' height: 1px; border-bottom: 2px solid ' +
              self.history[k][1].color + ';"></div>'
              + " " + v2 + '</span><br>'
            );
          } else {
            d3.select(selector).select('.distributionHeader').remove();
          }
        }, function() {
          d3.selectAll(selector + " .nv-bar").on('click',
            function() {
              // no idea why this has to be empty, well anyways...
            }
          );
        });
      });

    },

    stopUpdating: function () {
      this.isUpdating = false;
    },

    startUpdating: function () {
      var self = this;
      if (self.timer) {
        return;
      }
      self.timer = window.setInterval(function () {
        self.getStatistics();
        if (self.isUpdating === false) {
          return;
        }
        self.updateCharts();
      },
      self.interval
    );
  },


  resize: function () {
    if (!this.isUpdating) {
      return;
    }
    var self = this, dimensions;
      _.each(this.graphs,function (g) {
      dimensions = self.getCurrentSize(g.maindiv_.id);
      g.resize(dimensions.width, dimensions.height);
    });
    if (this.detailGraph) {
      dimensions = this.getCurrentSize(this.detailGraph.maindiv_.id);
      this.detailGraph.resize(dimensions.width, dimensions.height);
    }
    this.prepareD3Charts(true);
    this.prepareResidentSize(true);
  },

  template: templateEngine.createTemplate("dashboardView.ejs"),

  render: function (modalView) {
    if (!modalView)  {
      $(this.el).html(this.template.render());
    }
    this.getStatistics();
    this.prepareDygraphs();
    if (this.isUpdating) {
      this.prepareD3Charts();
      this.prepareResidentSize();
      this.updateTendencies();
    }
    this.startUpdating();
  }
});
}());
