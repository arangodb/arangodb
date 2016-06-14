/*global _, Dygraph, window, document */

(function () {
  "use strict";
  window.dygraphConfig = {
    defaultFrame : 20 * 60 * 1000,

    zeropad: function (x) {
      if (x < 10) {
        return "0" + x;
      }
      return x;
    },

    xAxisFormat: function (d) {
      if (d === -1) {
        return "";
      }
      var date = new Date(d);
      return this.zeropad(date.getHours()) + ":"
        + this.zeropad(date.getMinutes()) + ":"
        + this.zeropad(date.getSeconds());
    },

    mergeObjects: function (o1, o2, mergeAttribList) {
      if (!mergeAttribList) {
        mergeAttribList = [];
      }
      var vals = {}, res;
      mergeAttribList.forEach(function (a) {
        var valO1 = o1[a],
        valO2 = o2[a];
        if (valO1 === undefined) {
          valO1 = {};
        }
        if (valO2 === undefined) {
          valO2 = {};
        }
        vals[a] = _.extend(valO1, valO2);
      });
      res = _.extend(o1, o2);
      Object.keys(vals).forEach(function (k) {
        res[k] = vals[k];
      });
      return res;
    },

    mapStatToFigure : {
      pageFaults : ["times", "majorPageFaultsPerSecond", "minorPageFaultsPerSecond"],
      systemUserTime : ["times", "systemTimePerSecond", "userTimePerSecond"],
      totalTime : ["times", "avgQueueTime", "avgRequestTime", "avgIoTime"],
      dataTransfer : ["times", "bytesSentPerSecond", "bytesReceivedPerSecond"],
      requests : ["times", "getsPerSecond", "putsPerSecond", "postsPerSecond",
                  "deletesPerSecond", "patchesPerSecond", "headsPerSecond",
                  "optionsPerSecond", "othersPerSecond"]
    },

    //colors for dygraphs
    colors: ["rgb(95, 194, 135)", "rgb(238, 190, 77)", "#81ccd8", "#7ca530", "#3c3c3c",
             "#aa90bd", "#e1811d", "#c7d4b2", "#d0b2d4"],


    // figure dependend options
    figureDependedOptions: {
      clusterRequestsPerSecond: {
        showLabelsOnHighlight: true,
        title: '',
        header : "Cluster Requests per Second",
        stackedGraph: true,
        div: "lineGraphLegend",
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }

              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      pageFaults: {
        header : "Page Faults",
        visibility: [true, false],
        labels: ["datetime", "Major Page", "Minor Page"],
        div: "pageFaultsChart",
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      systemUserTime: {
        div: "systemUserTimeChart",
        header: "System and User Time",
        labels: ["datetime", "System Time", "User Time"],
        stackedGraph: true,
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      totalTime: {
        div: "totalTimeChart",
        header: "Total Time",
        labels: ["datetime", "Queue", "Computation", "I/O"],
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        },
        stackedGraph: true
      },

      dataTransfer: {
        header: "Data Transfer",
        labels: ["datetime", "Bytes sent", "Bytes received"],
        stackedGraph: true,
        div: "dataTransferChart"
      },

      requests: {
        header: "Requests",
        labels: ["datetime", "Reads", "Writes"],
        stackedGraph: true,
        div: "requestsChart",
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      }
    },

    getDashBoardFigures : function (all) {
      var result = [], self = this;
      Object.keys(this.figureDependedOptions).forEach(function (k) {
        // ClusterRequestsPerSecond should not be ignored. Quick Fix
        if (k !== "clusterRequestsPerSecond" && (self.figureDependedOptions[k].div || all)) {
          result.push(k);
        }
      });
      return result;
    },

    //configuration for chart overview
    getDefaultConfig: function (figure) {
      var self = this;
      var result = {
        digitsAfterDecimal: 1,
        drawGapPoints: true,
        fillGraph: true,
        fillAlpha: 0.85,
        showLabelsOnHighlight: false,
        strokeWidth: 0.0,
        lineWidth: 0.0,
        strokeBorderWidth: 0.0,
        includeZero: true,
        highlightCircleSize: 2.5,
        labelsSeparateLines : true,
        strokeBorderColor: 'rgba(0,0,0,0)',
        interactionModel: {},
        maxNumberWidth : 10,
        colors: [this.colors[0]],
        xAxisLabelWidth: "50",
        rightGap: 15,
        showRangeSelector: false,
        rangeSelectorHeight: 50,
        rangeSelectorPlotStrokeColor: '#365300',
        rangeSelectorPlotFillColor: '',
        // rangeSelectorPlotFillColor: '#414a4c',
        pixelsPerLabel: 50,
        labelsKMG2: true,
        dateWindow: [
          new Date().getTime() -
            this.defaultFrame,
          new Date().getTime()
        ],
        axes: {
          x: {
            valueFormatter: function (d) {
              return self.xAxisFormat(d);
            }
          },
          y: {
            ticker: Dygraph.numericLinearTicks
          }
        }
      };
      if (this.figureDependedOptions[figure]) {
        result = this.mergeObjects(
          result, this.figureDependedOptions[figure], ["axes"]
        );
        if (result.div && result.labels) {
          result.colors = this.getColors(result.labels);
          result.labelsDiv = document.getElementById(result.div + "Legend");
          result.legend = "always";
          result.showLabelsOnHighlight = true;
        }
      }
      return result;

    },

    getDetailChartConfig: function (figure) {
      var result = _.extend(
        this.getDefaultConfig(figure),
        {
          showRangeSelector: true,
          interactionModel: null,
          showLabelsOnHighlight: true,
          highlightCircleSize: 2.5,
          legend: "always",
          labelsDiv: "div#detailLegend.dashboard-legend-inner"
        }
      );
      if (figure === "pageFaults") {
        result.visibility = [true, true];
      }
      if (!result.labels) {
        result.labels = ["datetime", result.header];
        result.colors = this.getColors(result.labels);
      }
      return result;
    },

    getColors: function (labels) {
      var colorList;
      colorList = this.colors.concat([]);
      return colorList.slice(0, labels.length - 1);
    }
  };
}());
