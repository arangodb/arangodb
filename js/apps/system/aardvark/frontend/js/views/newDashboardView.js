/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, console, Dygraph, _,templateEngine */

(function() {
  "use strict";

  window.newDashboardView = Backbone.View.extend({
    el: '#content',
    interval: 15000, // in milliseconds
    defaultFrame : 20 * 60 * 1000,
    defaultDetailFrame : 14 *24 * 60 * 60 * 1000,
    history : {},
    graphs  : {},
    alreadyCalledDetailChart : [],

    events: {
      "click .dashboard-chart" : "showDetail",
      "mousedown .dygraph-rangesel-zoomhandle" : "stopUpdating",
      "mouseup .dygraph-rangesel-zoomhandle" : "startUpdating",
      "mouseleave .dygraph-rangesel-zoomhandle" : "startUpdating",
      "click #backToCluster" : "returnToClusterView"
    },

    tendencies : {

      asyncRequestsCurrent : ["asyncRequestsCurrent", "asyncRequestsCurrentPercentChange"],
      asyncRequestsAverage : ["asyncPerSecond15M", "asyncPerSecondPercentChange15M"],
      clientConnectionsCurrent : ["clientConnectionsCurrent",
          "clientConnectionsCurrentPercentChange"
      ],
      clientConnectionsAverage : ["clientConnections15M", "clientConnectionsPercentChange15M"],
      numberOfThreadsCurrent : ["numberOfThreadsCurrent", "numberOfThreadsCurrentPercentChange"],
      numberOfThreadsAverage : ["numberOfThreads15M", "numberOfThreadsPercentChange15M"],
      virtualSizeCurrent : ["virtualSizeCurrent", "virtualSizePercentChange"],
      virtualSizeAverage : ["virtualSize15M", "virtualSizePercentChange15M"]
    },

    showDetail : function(e) {
        var self = this;
        var figure = $(e.currentTarget).attr("id").replace(/ChartContainer/g , "");
        this.getStatistics(figure);
        var options = this.dygraphConfig.getDetailChartConfig(figure);

        this.detailGraphFigure = figure;
        window.modalView.show(
            "modalGraph.ejs",
            options.header,
            [window.modalView.createCloseButton(
                    this.hidden.bind(this)
                )
            ]
        );

        $('#modal-dialog').toggleClass("modalChartDetail", true);
        $('#modal-body').toggleClass("modal-body", true);

        var dimensions = self.getCurrentSize("#lineChartDetail");
        options.height = dimensions.height;
        options.width = dimensions.width;

        self.detailGraph = new Dygraph(
            document.getElementById("lineChartDetail"),
            self.history[figure],
            options
        );

    },

    hidden: function () {
        this.detailGraph.destroy();
        delete this.detailGraph;
        delete this.detailGraphFigure;
    },


    getCurrentSize : function (div) {
        var height, width;
        height = $(div).height();
        width = $(div).width();
        return {
            height : height,
            width: width
        };
    },

    prepareDygraphs : function () {
        var self = this, options;
        this.dygraphConfig.getDashBoardFigures().forEach(function (f) {
            options = self.dygraphConfig.getDefaultConfig(f);
            var dimensions = self.getCurrentSize(options.div);
            options.height = dimensions.height;
            options.width = dimensions.width;
            self.graphs[f] = new Dygraph(
                document.getElementById(options.div),
                self.history[f],
                options
            );
        });
    },

    initialize: function () {
        this.dygraphConfig = this.options.dygraphConfig;

    },

    updateCharts : function () {
        var self = this;
        if (this.detailGraph) {
            this.updateLineChart(this.detailGraphFigure, true);
            return;
        }
        this.updateD3Charts();
        this.updateTendencies();
        Object.keys(this.graphs).forEach(function(f) {
            self.updateLineChart(f);
        });
    },

    updateTendencies : function () {
        var self = this, map = this.tendencies;

        Object.keys(map).forEach(function (a) {
            $("#" + a).text(self.history[a][0] + " (" + self.history[a][1] + " %)");
        });
    },


    updateD3Charts : function () {

    },

    prepareD3Charts : function () {

    },

    updateDateWindow : function(graph, isDetailChart) {
        var t = new Date().getTime();
        var borderLeft, borderRight;
    /*    if (isDetailChart) {
        } else {
        }
    */
        return [new Date().getTime() - this.defaultFrame, new Date().getTime()];


    },

    updateLineChart : function (figure, isDetailChart) {

        var opts = {
            file: this.history[figure],
            dateWindow : this.updateDateWindow(isDetailChart)
        };
        if (isDetailChart) {
            this.detailGraph.updateOptions(opts);
        } else {
            this.graphs[figure].updateOptions(opts);
        }
    },

    mergeDygraphHistory : function (newData, i) {
        var self = this, valueList;
        this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {
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

    mergeHistory : function (newData) {
        var self = this, i;
        for (i = 0;  i < newData.times.length;  ++i) {
            this.mergeDygraphHistory(newData, i);
        }
        Object.keys(this.tendencies).forEach(function (a) {
            if (a === "virtualSizeCurrent" ||
                a === "virtualSizeAverage") {
                newData[self.tendencies[a][0]] =
                    newData[self.tendencies[a][0]] / (1024 * 1024 * 1024);
            }
            self.history[a] = [
                Math.round(newData[self.tendencies[a][0]] * 1000) / 1000,
                Math.round(newData[self.tendencies[a][1]] * 1000) / 1000
            ];
        });
        this.nextStart = newData.nextStart;
    },

    getStatistics : function (figure) {
        var url = "statistics/full?start=", self = this, dateString;
        if (!figure && this.nextStart) {
            url+= this.nextStart;
        } else if (!figure && !this.nextStart) {
            url+= (new Date().getTime() - this.defaultFrame)/ 1000;
        } else {
            if ( this.alreadyCalledDetailChart.indexOf(figure) !== -1) {
                return;
            }
            this.history[figure] = [];
            url+=  (new Date().getTime() - this.defaultDetailFrame)/ 1000;
            url+= "&filter=" + this.dygraphConfig.mapStatToFigure[figure].join();
            this.alreadyCalledDetailChart.push(figure);
        }
        console.log(url);
        $.ajax(
            url,
            {async: false}
        ).done(
            function(d) {
                console.log("got result");
                self.mergeHistory(d);
            }
        );

    },

    stopUpdating: function () {
      this.isUpdating = false;
    },

    startUpdating: function () {
      var self = this;
      if (self.isUpdating) {
        return;
      }
      self.isUpdating = true;
      self.timer = window.setInterval(function() {
              self.getStatistics();
              self.updateCharts();
      },
      self.interval
    );
  },


  resize: function() {
      var self = this, dimensions;
      Object.keys(this.graphs).forEach(function (f) {
          var g = self.graphs[f];
          dimensions = self.getCurrentSize(g.maindiv_.id);
          g.resize(dimensions.width , dimensions.height);
      });
  },

  template: templateEngine.createTemplate("newDashboardView.ejs"),

  render: function() {
    $(this.el).html(this.template.render());
    this.getStatistics();
    this.prepareDygraphs();
    this.prepareD3Charts();
    this.updateTendencies();
    this.startUpdating();
  },



  returnToClusterView : function() {
    window.history.back();
  }
});
}());
