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
    interval: 1000000, // in milliseconds
    defaultTimeFrame: 20 * 60 * 1000, // 20 minutes in milliseconds
    defaultDetailFrame: 2 * 24 * 60 * 60 * 1000,
    history: {},
    graphs: {},
    alreadyCalledDetailChart: [],

    events: {
      "click .dashboard-large-chart-menu": "showDetail"
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
      // var figure = $(e.currentTarget).attr("id").replace(/ChartContainer/g, "");
      var figure = $(e.currentTarget.parentNode).attr("id").replace(/ChartContainer/g, "");
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

			window.modalView.hideFooter = false;

      $('#modal-dialog').on('hidden', function () {
        self.hidden();
      });

      $('#modal-dialog').toggleClass("modal-chart-detail", true);

      options.height = $(window).height() * 0.7;
      options.width = $('.modal-inner-detail').width();
      
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
      this.events["click .dashboard-large-chart-menu"] = this.showDetail.bind(this);
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
        $("#" + a).html(self.history[a][0] + lineBreak + '<span class="dashboard-figurePer"><span style="color: '+ tempColor +';">' + self.history[a][1] + '%</span></span>');
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
      return [t - this.defaultTimeFrame, t];


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

        // check if figure is known
        if (! self.dygraphConfig.mapStatToFigure[f]) {
          return;
        }

        // need at least an empty history
        if (! self.history[f]) {
          self.history[f] = [];
        }

        // generate values for this key
        valueList = [];

        self.dygraphConfig.mapStatToFigure[f].forEach(function (a) {
          if (! newData[a]) {
            return;
          }

          if (a === "times") {
            valueList.push(new Date(newData[a][i] * 1000));
          } else {
            valueList.push(newData[a][i]);
          }
        });

        // if we found at list one value besides times, then use the entry
        if (valueList.length > 1) {
          self.history[f].push(valueList);
        }
      });
    },

    cutOffHistory: function (f, cutoff) {
      var self = this;

      while (self.history[f].length !== 0) {
        var v = self.history[f][0][0];

        if (v >= cutoff) {
          break;
        }

        self.history[f].shift();
      }
    },

    cutOffDygraphHistory: function (cutoff) {
      var self = this;
      var cutoffDate = new Date(cutoff);

      this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {

        // check if figure is known
        if (! self.dygraphConfig.mapStatToFigure[f]) {
          return;
        }

        // history must be non-empty
        if (! self.history[f]) {
          return;
        }

        self.cutOffHistory(f, cutoffDate);
      });
    },

    mergeHistory: function (newData, detailMode) {
      var self = this, i;

      for (i = 0; i < newData.times.length; ++i) {
        this.mergeDygraphHistory(newData, i);
      }

      this.cutOffDygraphHistory(new Date().getTime() - this.defaultTimeFrame);

      if (detailMode) {
        return;
      }

      // convert tendency values
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

      // update distribution
      Object.keys(this.barCharts).forEach(function (a) {
        self.history[a] = self.mergeBarChartData(self.barCharts[a], newData);
      });

      // update physical memory
      self.history.physicalMemory = newData.physicalMemory;

      // generate chart description
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

      // remember next start
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
      for (i = newData[attribList[0]].values.length - 1;  0 <= i;  --i) {
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
        urlParams += (new Date().getTime() - this.defaultTimeFrame) / 1000;
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
        var currentA = fmtNumber((current / 1024), 2) + " GB"
        
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
            right: 1,
            bottom: ($(residentSizeChartContainer).outerHeight() - $(residentSizeChartContainer).height()) / 2,
            left: 1
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

        var selector = "#" + k + "Container svg";
        var dist = dists[k];

        nv.addGraph(function () {
					var tickMarks = [0, 0.25, 0.5, 0.75, 1];  
					var marginLeft = 75;
					var marginBottom = 23;
					var bottomSpacer = 6;

					if (dimensions.width < 219) {
						tickMarks = [0, 0.5, 1];  
						marginLeft = 72;
						marginBottom = 21;
						bottomSpacer = 5;
					}
					else if (dimensions.width < 299) {
						tickMarks = [0, 0.3334, 0.6667, 1];  
						marginLeft = 75;
					}
					else if (dimensions.width < 379) {
						marginLeft = 83;
					}
					else if (dimensions.width < 459) {
						marginLeft = 85;
					}
					else if (dimensions.width < 539) {
						marginLeft = 95;
					}
					else if (dimensions.width < 619) {
						marginLeft = 98;
					}

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
              top: 5,
              right: 20,
              bottom: marginBottom,
              left: marginLeft
            })
            .showValues(false)
            .showYAxis(true)
            .showXAxis(true)
            .transitionDuration(100)
            .tooltips(false)
            .showLegend(false)
            .showControls(false)
						.forceY([0,1]);
          
					chart.yAxis
						.showMaxMin(false);
					
					var yTicks2 = d3.select('.nv-y.nv-axis')
						.selectAll('text')
						.attr('transform', 'translate (0, ' + bottomSpacer + ')') ;
          
          chart.yAxis
						.tickValues(tickMarks)
						.tickFormat(function (d) {return fmtNumber(((d * 100 * 100) / 100), 0) + "%";});

					d3.select(selector)
            .datum(self.history[k])
            .call(chart);

          nv.utils.windowResize(chart.update);
          
          return chart;
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
