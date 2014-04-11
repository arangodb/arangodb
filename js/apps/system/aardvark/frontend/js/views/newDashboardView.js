/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, Dygraph, _,templateEngine */

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
      clientConnectionsCurrent : ["clientConnectionsCurrent", "clientConnectionsCurrentPercentChange"],
      clientConnectionsAverage : ["clientConnections15M", "clientConnectionsPercentChange15M"],
      numberOfThreadsCurrent : ["numberOfThreadsCurrent", "numberOfThreadsCurrentPercentChange"],
      numberOfThreadsAverage : ["numberOfThreads15M", "numberOfThreadsPercentChange15M"],
      virtualSizeCurrent : ["virtualSizeCurrent", "virtualSizePercentChange"],
      virtualSizeAverage : ["virtualSize15M", "virtualSizePercentChange15M"]
    },

    showDetail : function(e) {
        var self = this;
        var figure = $(e.currentTarget).attr("id").replace(/ChartContainer/g , "");
        this.getStatistics(figure)
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
        }
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
            return
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



        /*$( "div.second" ).replaceWith( "<h2>New heading</h2>" );$( "div.second" ).replaceWith( "<h2>New heading</h2>" );*/
    },


    updateD3Charts : function () {

    },

    prepareD3Charts : function () {

    },

    updateDateWindow : function(graph, isDetailChart) {
        var t = new Date().getTime();
        var borderLeft, borderRight;
        if (isDetailChart) {
         /*   displayOptions.height = $('#lineChartDetail').height() - 34 -29;
            displayOptions.width = $('#lineChartDetail').width() -10;
            chart.detailChartConfig();
            if (chart.graph.dateWindow_) {
                borderLeft = chart.graph.dateWindow_[0];
                borderRight = t - chart.graph.dateWindow_[1] - self.interval * 5 > 0 ?
                    chart.graph.dateWindow_[1] : t;
            }*/
        } else {
           /* chart.updateDateWindow();
            borderLeft = chart.options.dateWindow[0];
            borderRight = chart.options.dateWindow[1];*/
        }
        /*if (self.modal && createDiv) {
            displayOptions.height = $('.innerDashboardChart').height() - 34;
            displayOptions.width = $('.innerDashboardChart').width() -45;
        }*/

        return [new Date().getTime() - this.defaultFrame, new Date().getTime()]


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


    mergeHistory : function (newData) {
        var self = this, valueList, i, figures = this.dygraphConfig.getDashBoardFigures(true);
        /*Object.keys(this.tendencies).forEach(function (a) {
          figures.push(self.tendencies[a][0])
          figures.push(self.tendencies[a][1])
        });*/


        for (i = 0;  i < newData["times"].length;  ++i) {
            figures.forEach(function (f) {
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
        };
        Object.keys(this.tendencies).forEach(function (a) {
            if (a === "virtualSizeCurrent" ||
                a === "virtualSizeAverage") {
                newData[self.tendencies[a][0]] = newData[self.tendencies[a][0]] / (1024 * 1024 * 1024);
            } else if (a === "clientConnectionsCurrent" ||
                a === "numberOfThreadsCurrent") {
                newData[self.tendencies[a][0]] = newData[self.tendencies[a][0]];
            }
            self.history[a] = [
                Math.round(newData[self.tendencies[a][0]] * 1000) / 1000,
                Math.round(newData[self.tendencies[a][1]] * 1000) / 1000
            ];
        });
        this.nextStart = newData.nextStart;
    },

    getStatistics : function (figure) {
        var url = "statistics/full?", self = this, d;
        if (!figure) {
            d = this.nextStart ? this.nextStart : (new Date().getTime() - this.defaultFrame);
            url+= "start=" +  (this.nextStart ? this.nextStart : (new Date().getTime() - this.defaultFrame)/ 1000);
        } else {
            if ( this.alreadyCalledDetailChart.indexOf(figure) !== -1) {
                return;
            }
            this.history[figure] = [];
            d = new Date().getTime() - this.defaultDetailFrame;
            url+=  "start=" + (new Date().getTime() - this.defaultDetailFrame)/ 1000;
            url+= "&filter=" + this.dygraphConfig.mapStatToFigure[figure].join();
            this.alreadyCalledDetailChart.push(figure);
        }
        $.ajax(
            url,
            {async: false}
        ).done(
            function(d) {
                console.log(d);
                self.mergeHistory(d)
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
