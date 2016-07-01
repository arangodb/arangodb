/* jshint browser: true */
/* jshint unused: false */
/* global describe, jasmine, beforeEach, afterEach, Backbone, it, spyOn, expect*/
/* global $*/

(function () {
  'use strict';

  describe('dashBoardConfig', function () {
    var col;

    beforeEach(function () {
      col = window.dygraphConfig;
    });

    it('zeropad', function () {
      expect(col.defaultFrame).toEqual(20 * 60 * 1000);
      expect(col.zeropad(9)).toEqual('09');
      expect(col.zeropad(11)).toEqual(11);
    });

    it('xAxisFormat', function () {
      expect(col.xAxisFormat(-1)).toEqual('');
      var d = new Date();
      expect(col.xAxisFormat(d.getTime())).toEqual(col.zeropad(d.getHours()) + ':'
        + col.zeropad(d.getMinutes()) + ':'
        + col.zeropad(d.getSeconds()));
    });

    it('mergeObjects', function () {
      var o1 = {
          a: 1,
          b: {
            x: 2
          }
        },
        o2 = {
          a: 2,
          c: {
            x: 2
          }
      };

      expect(col.mergeObjects(o1, o2, ['b', 'c'])).toEqual(
        {
          a: 2,
          b: {
            x: 2
          },
          c: {
            x: 2
          }
        }

      );
    });

    it('mergeObjects with empty attrib list', function () {
      var o1 = {
          a: 1,
          b: {
            x: 2
          }
        },
        o2 = {
          a: 2,
          b: {
            x: 2
          }
      };

      expect(col.mergeObjects(o1, o2)).toEqual(
        {
          a: 2,
          b: {
            x: 2
          }
        }

      );
    });

    it('check constants', function () {
      var stats = col.mapStatToFigure,
        options = col.figureDependedOptions,
        reqps = options.clusterRequestsPerSecond,
        floatNum = 123.456789,
        formatter;
      expect(stats.residentSize).toEqual(['times', 'residentSizePercent']);
      expect(stats.pageFaults).toEqual(
        ['times', 'majorPageFaultsPerSecond', 'minorPageFaultsPerSecond']
      );
      expect(stats.systemUserTime).toEqual(
        ['times', 'systemTimePerSecond', 'userTimePerSecond']
      );
      expect(stats.totalTime).toEqual(
        ['times', 'avgQueueTime', 'avgRequestTime', 'avgIoTime']
      );
      expect(stats.dataTransfer).toEqual(
        ['times', 'bytesSentPerSecond', 'bytesReceivedPerSecond']
      );
      expect(stats.requests).toEqual(
        [
          'times', 'getsPerSecond', 'putsPerSecond', 'postsPerSecond',
          'deletesPerSecond', 'patchesPerSecond', 'headsPerSecond',
          'optionsPerSecond', 'othersPerSecond'
        ]
      );

      expect(col.colors).toEqual(
        ['#617e2b', '#296e9c', '#81ccd8', '#7ca530', '#3c3c3c',
          '#aa90bd', '#e1811d', '#c7d4b2', '#d0b2d4']
      );

      expect(reqps.showLabelsOnHighlight).toEqual(true);
      expect(reqps.title).toEqual('');
      expect(reqps.header).toEqual('Cluster Requests per Second');
      expect(reqps.stackedGraph).toEqual(true);
      expect(reqps.div).toEqual('lineGraphLegend');
      expect(reqps.labelsKMG2).toEqual(false);

      expect(reqps.axes).toBeDefined();
      expect(reqps.axes.y).toBeDefined();
      expect(reqps.axes.y.valueFormatter).toBeDefined();
      formatter = reqps.axes.y.valueFormatter;
      expect(formatter(floatNum)).toEqual(parseFloat(floatNum.toPrecision(3)));

      expect(reqps.axes.y.axisLabelFormatter).toBeDefined();
      formatter = reqps.axes.y.axisLabelFormatter;
      expect(formatter(floatNum)).toEqual(parseFloat(floatNum.toPrecision(3)));
      expect(formatter(0)).toEqual(0);
    });

    it('getDashBoardFigures', function () {
      var result = [];
      Object.keys(col.figureDependedOptions).forEach(function (k) {
        if (k !== 'clusterRequestsPerSecond'
          && col.figureDependedOptions[k].div) {
          result.push(k);
        }
      });
      expect(col.getDashBoardFigures()).toEqual(result);
    });

    it('get all DashBoardFigures', function () {
      var result = [], self = this;
      Object.keys(col.figureDependedOptions).forEach(function (k) {
        if (k !== 'clusterRequestsPerSecond') {
          result.push(k);
        }
      });
      expect(col.getDashBoardFigures(true)).toEqual(result);
    });

    it('getDefaultConfig', function () {
      var res = col.getDefaultConfig('systemUserTime');
      expect(res.digitsAfterDecimal).toEqual(1);
      expect(res.drawGapPoints).toEqual(true);
      expect(res.fillGraph).toEqual(true);
      expect(res.showLabelsOnHighlight).toEqual(true);
      expect(res.strokeWidth).toEqual(1.5);
      expect(res.strokeBorderWidth).toEqual(1.5);
      expect(res.includeZero).toEqual(true);
      expect(res.highlightCircleSize).toEqual(2.5);
      expect(res.labelsSeparateLines).toEqual(true);
      expect(res.strokeBorderColor).toEqual('#ffffff');
      expect(res.interactionModel).toEqual({});
      expect(res.maxNumberWidth).toEqual(10);
      expect(res.colors).toEqual([ '#617e2b', '#296e9c' ]);
      expect(res.xAxisLabelWidth).toEqual('50');
      expect(res.rightGap).toEqual(15);
      expect(res.showRangeSelector).toEqual(false);
      expect(res.rangeSelectorHeight).toEqual(50);
      expect(res.rangeSelectorPlotStrokeColor).toEqual('#365300');
      // expect(res.rangeSelectorPlotFillColor).toEqual('#414a4c')
      expect(res.pixelsPerLabel).toEqual(50);
      expect(res.labelsKMG2).toEqual(false);
      expect(res.axes).toEqual({
        x: {
          valueFormatter: jasmine.any(Function)
        },
        y: {
          valueFormatter: jasmine.any(Function),
          axisLabelFormatter: jasmine.any(Function)
        }
      });
      expect(res.div).toEqual('systemUserTimeChart');
      expect(res.header).toEqual('System and User Time');
      expect(res.labels).toEqual(['datetime',
        'System Time', 'User Time']);
      expect(res.labelsDiv).toEqual(null);
      expect(res.legend).toEqual('always');
      expect(res.axes.x.valueFormatter(-1)).toEqual(
        col.xAxisFormat(-1));
    });

    it('getDetailConfig', function () {
      var res = col.getDetailChartConfig('pageFaults');
      expect(res.digitsAfterDecimal).toEqual(1);
      expect(res.drawGapPoints).toEqual(true);
      expect(res.fillGraph).toEqual(true);
      expect(res.showLabelsOnHighlight).toEqual(true);
      expect(res.strokeWidth).toEqual(1.5);
      expect(res.strokeBorderWidth).toEqual(1.5);
      expect(res.includeZero).toEqual(true);
      expect(res.highlightCircleSize).toEqual(2.5);
      expect(res.labelsSeparateLines).toEqual(true);
      expect(res.strokeBorderColor).toEqual('#ffffff');
      expect(res.interactionModel).toEqual(null);
      expect(res.maxNumberWidth).toEqual(10);
      expect(res.colors).toEqual([ '#617e2b', '#296e9c' ]);
      expect(res.xAxisLabelWidth).toEqual('50');
      expect(res.rightGap).toEqual(15);
      expect(res.showRangeSelector).toEqual(true);
      expect(res.rangeSelectorHeight).toEqual(50);
      expect(res.rangeSelectorPlotStrokeColor).toEqual('#365300');
      // expect(res.rangeSelectorPlotFillColor).toEqual('#414a4c')
      expect(res.pixelsPerLabel).toEqual(50);
      expect(res.labelsKMG2).toEqual(false);
      expect(res.div).toEqual('pageFaultsChart');
      expect(res.header).toEqual('Page Faults');
      expect(res.labels).toEqual(['datetime', 'Major Page',
        'Minor Page']);
      expect(res.legend).toEqual('always');
      expect(res.visibility).toEqual([true, true]);
    });

    it('getDetailConfig with empty labels', function () {
      var res = col.getDetailChartConfig('requestsAsync');
      expect(res.digitsAfterDecimal).toEqual(1);
      expect(res.drawGapPoints).toEqual(true);
      expect(res.fillGraph).toEqual(true);
      expect(res.showLabelsOnHighlight).toEqual(true);
      expect(res.strokeWidth).toEqual(1.5);
      expect(res.strokeBorderWidth).toEqual(1.5);
      expect(res.includeZero).toEqual(true);
      expect(res.highlightCircleSize).toEqual(2.5);
      expect(res.labelsSeparateLines).toEqual(true);
      expect(res.strokeBorderColor).toEqual('#ffffff');
      expect(res.interactionModel).toEqual(null);
      expect(res.maxNumberWidth).toEqual(10);
      expect(res.colors).toEqual([ '#617e2b' ]);
      expect(res.xAxisLabelWidth).toEqual('50');
      expect(res.rightGap).toEqual(15);
      expect(res.showRangeSelector).toEqual(true);
      expect(res.rangeSelectorHeight).toEqual(50);
      expect(res.rangeSelectorPlotStrokeColor).toEqual('#365300');
      // expect(res.rangeSelectorPlotFillColor).toEqual('#414a4c')
      expect(res.pixelsPerLabel).toEqual(50);
      expect(res.labelsKMG2).toEqual(true);
      expect(res.legend).toEqual('always');
    });
  });
}());
