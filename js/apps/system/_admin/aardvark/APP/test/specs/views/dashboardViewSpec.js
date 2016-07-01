/* jshint browser: true */
/* jshint unused: false */
/* global $, arangoHelper, jasmine, nv, d3, describe, beforeEach, afterEach, it, spyOn, expect */
/* global this.options, this.options.database, this.options.database.findWhere */

(function () {
  'use strict';

  describe('The dashboard view', function () {
    var view, dyGraphConfigDummy, modalDummy, d3ChartDummy, databaseDummy;

    beforeEach(function () {
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        }
      };
      window.CreateDummyForObject(window, 'ArangoDatabase');
      databaseDummy = new window.ArangoDatabase();
      dyGraphConfigDummy = {
        getDetailChartConfig: function () {
          return {
            header: 'dummyheader'
          };
        },
        getDashBoardFigures: function () {
          return ['a', 'b', 'c'];
        },
        getDefaultConfig: function (d) {
          return {
            header: 'dummyheader',
            div: '#' + d
          };
        },
        mapStatToFigure: {
          a: ['times', 'x', 'blub'],
          d: ['times', 'y'],
          c: ['times', 'z'],
          abc: [1]
        },

        colors: [1, 2]

      };
      modalDummy = {
        hide: function () {
          throw 'Should be a spy';
        },
        show: function () {
          throw 'Should be a spy';
        }
      };
      window.modalView = modalDummy;

      d3ChartDummy = {
        x: function (a) {
          a({label: 1});
          return d3ChartDummy;
        },
        y: function (a) {
          a({label: 1});
          return d3ChartDummy;
        },
        width: function () {
          return d3ChartDummy;
        },
        height: function () {
          return d3ChartDummy;
        },
        margin: function () {
          return d3ChartDummy;
        },
        showValues: function () {
          return d3ChartDummy;
        },
        showYAxis: function () {
          return d3ChartDummy;
        },
        showXAxis: function () {
          return d3ChartDummy;
        },
        transitionDuration: function () {
          return d3ChartDummy;
        },
        tooltips: function () {
          return d3ChartDummy;
        },
        showLegend: function () {
          return d3ChartDummy;
        },
        stacked: function () {
          return d3ChartDummy;
        },
        showControls: function () {
          return d3ChartDummy;
        },

        classed: function () {
          return d3ChartDummy;
        },

        forceY: function () {
          return d3ChartDummy;
        },

        yAxis: {
          tickFormat: function (a) {
            a();
            return d3ChartDummy.yAxis;
          },

          showMaxMin: function () {
            return d3ChartDummy.yAxis;
          },
          tickValues: function () {
            return d3ChartDummy.yAxis;
          }
        },

        xAxis: {
          tickFormat: function (a) {
            a();
          },

          showMaxMin: function () {}
        },

        datum: function () {
          return d3ChartDummy;
        },
        call: function () {
          return d3ChartDummy;
        },
        data: function () {
          return d3ChartDummy;
        },
        enter: function () {
          return d3ChartDummy;
        },
        append: function () {
          return d3ChartDummy;
        },
        style: function () {
          return d3ChartDummy;
        },
        attr: function () {
          return d3ChartDummy;
        },
        text: function (a) {
          if (a instanceof Function) {
            a();
          }
          return d3ChartDummy;
        },
        select: function () {
          return d3ChartDummy;
        },
        remove: function () {
          return d3ChartDummy;
        },
        selectAll: function () {
          return d3ChartDummy;
        },
        on: function () {
          return d3ChartDummy;
        }

      };

      view = new window.DashboardView({dygraphConfig: dyGraphConfigDummy, database: databaseDummy});
    });

    afterEach(function () {
      delete window.App;
    });

    it('assert the basics', function () {
      expect(view.interval).toEqual(10000);
      expect(view.defaultTimeFrame).toEqual(20 * 60 * 1000);
      expect(view.defaultDetailFrame).toEqual(2 * 24 * 60 * 60 * 1000);
      expect(view.history).toEqual({'-local-': {}});
      expect(view.graphs).toEqual({});

      expect(view.events).toEqual({
        'click .dashboard-sub-bar-menu-sign': jasmine.any(Function),
        'mousedown .dygraph-rangesel-zoomhandle': jasmine.any(Function),
        'mouseup .dygraph-rangesel-zoomhandle': jasmine.any(Function)
      });

      var tend = view.tendencies,
        bar = view.barCharts,
        barNames = view.barChartsElementNames;
      expect(tend.asyncPerSecondCurrent).toEqual([
        'asyncPerSecondCurrent', 'asyncPerSecondPercentChange'
      ]);
      expect(tend.syncPerSecondCurrent).toEqual([
        'syncPerSecondCurrent', 'syncPerSecondPercentChange'
      ]);
      expect(tend.clientConnectionsCurrent).toEqual([
        'clientConnectionsCurrent', 'clientConnectionsPercentChange'
      ]);
      expect(tend.clientConnectionsAverage).toEqual([
        'clientConnections15M', 'clientConnections15MPercentChange'
      ]);
      expect(tend.numberOfThreadsCurrent).toEqual([
        'numberOfThreadsCurrent', 'numberOfThreadsPercentChange'
      ]);
      expect(tend.numberOfThreadsAverage).toEqual([
        'numberOfThreads15M', 'numberOfThreads15MPercentChange'
      ]);
      expect(tend.virtualSizeCurrent).toEqual([
        'virtualSizeCurrent', 'virtualSizePercentChange'
      ]);
      expect(tend.virtualSizeAverage).toEqual([
        'virtualSize15M', 'virtualSize15MPercentChange'
      ]);

      expect(bar.totalTimeDistribution).toEqual([
        'queueTimeDistributionPercent', 'requestTimeDistributionPercent'
      ]);
      expect(bar.dataTransferDistribution).toEqual([
        'bytesSentDistributionPercent', 'bytesReceivedDistributionPercent'
      ]);

      expect(barNames.queueTimeDistributionPercent).toEqual('Queue');
      expect(barNames.requestTimeDistributionPercent).toEqual('Computation');
      expect(barNames.bytesSentDistributionPercent).toEqual('Bytes sent');
      expect(barNames.bytesReceivedDistributionPercent).toEqual('Bytes received');
    });

    it('getDetailFigure', function () {
      var jQueryDummy = {
        attr: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(jQueryDummy, 'attr').andReturn('asyncRequestsChartContainer');
      expect(view.getDetailFigure({currentTarget: ''})).toEqual('asyncRequestsChartContainer');
    });
    it('getDetailFigure for clientConnections', function () {
      var jQueryDummy = {
        attr: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(jQueryDummy, 'attr').andReturn('clientConnectionsDistributionContainer');
      expect(view.getDetailFigure({currentTarget: ''})).toEqual(
        'clientConnectionsDistributionContainer'
      );
    });

    /* it("showDetail", function () {
     spyOn(view, "getDetailFigure").andReturn("requestsAsync")
     spyOn(view, "getStatistics")
     spyOn(modalDummy, "hide")
     spyOn(modalDummy, "show")
     var jQueryDummy = {
       attr: function () {

       },
       on: function (a, b) {
         b()
       },
       toggleClass: function (a, b) {

       },
       height: function () {
         return 100
       },
       width: function () {
         return 100
       }

     }
     spyOn(window, "$").andReturn(
       jQueryDummy
     )
     spyOn(window, "Dygraph")
     spyOn(dyGraphConfigDummy, "getDetailChartConfig").andCallThrough()
     spyOn(jQueryDummy, "on").andCallThrough()
     spyOn(jQueryDummy, "toggleClass")
     spyOn(jQueryDummy, "height").andCallThrough()
     spyOn(jQueryDummy, "width").andCallThrough()
     spyOn(view, "hidden")

     view.showDetail("")

     expect(view.getDetailFigure).toHaveBeenCalledWith("")
     expect(view.getStatistics).toHaveBeenCalledWith("requestsAsync")
     expect(view.getDetailFigure).toHaveBeenCalledWith("")
     expect(view.detailGraphFigure).toEqual("requestsAsync")
     expect(dyGraphConfigDummy.getDetailChartConfig).toHaveBeenCalledWith("requestsAsync")
     expect(modalDummy.hide).toHaveBeenCalled()
     expect(jQueryDummy.height).toHaveBeenCalled()
     expect(jQueryDummy.width).toHaveBeenCalled()
     expect(window.$).toHaveBeenCalledWith('#modal-dialog')
     expect(window.$).toHaveBeenCalledWith('.modal-chart-detail')
     expect(jQueryDummy.on).toHaveBeenCalledWith('hidden', jasmine.any(Function))
     expect(jQueryDummy.toggleClass).toHaveBeenCalledWith("modal-chart-detail", true)
     expect(modalDummy.show).toHaveBeenCalledWith("modalGraph.ejs",
       "dummyheader",
       undefined,
       undefined,
       undefined,
       view.events
     )
     expect(window.Dygraph).toHaveBeenCalledWith(
       document.getElementById("lineChartDetail"),
       undefined,
       {
         header: "dummyheader",
         height: 70,
         width: 84
       })

       });*/

    it('hidden', function () {
      view.detailGraph = {
        destroy: function () {
          throw 'Should be a spy';
        }
      };
      view.detailGraphFigure = 1;
      spyOn(view.detailGraph, 'destroy');
      view.hidden();
      expect(view.detailGraph).toEqual(undefined);
      expect(view.detailGraphFigure).toEqual(undefined);
    });

    it('getCurrentSize', function () {
      var jQueryDummy = {
        attr: function () {
          throw 'Should be a spy';
        },
        height: function () {
          return 100;
        },
        width: function () {
          return 100;
        }
      };
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(jQueryDummy, 'attr');
      spyOn(jQueryDummy, 'height').andCallThrough();
      spyOn(jQueryDummy, 'width').andCallThrough();

      expect(view.getCurrentSize('anyDiv')).toEqual({height: 100, width: 100});

      expect(jQueryDummy.height).toHaveBeenCalled();
      expect(jQueryDummy.width).toHaveBeenCalled();
      expect(window.$).toHaveBeenCalledWith('#anyDiv');
      expect(jQueryDummy.attr).toHaveBeenCalledWith('style', '');
    });

    it('getCurrentSize', function () {
      spyOn(dyGraphConfigDummy, 'getDashBoardFigures').andCallThrough();
      spyOn(dyGraphConfigDummy, 'getDefaultConfig').andCallThrough();
      spyOn(view, 'getCurrentSize').andReturn({height: 80, width: 100});
      spyOn(window, 'Dygraph');
      view.prepareDygraphs();
      expect(dyGraphConfigDummy.getDashBoardFigures).toHaveBeenCalled();
      expect(dyGraphConfigDummy.getDefaultConfig).toHaveBeenCalledWith('a');
      expect(dyGraphConfigDummy.getDefaultConfig).toHaveBeenCalledWith('b');
      expect(dyGraphConfigDummy.getDefaultConfig).toHaveBeenCalledWith('c');
      expect(view.getCurrentSize).toHaveBeenCalledWith('#a');
      expect(view.getCurrentSize).toHaveBeenCalledWith('#b');
      expect(view.getCurrentSize).toHaveBeenCalledWith('#c');
      expect(window.Dygraph).toHaveBeenCalledWith(
        document.getElementById('#a'),
        [],
        {
          header: 'dummyheader',
          div: '#a',
          height: 80,
          width: 100
        }
      );
      expect(window.Dygraph).toHaveBeenCalledWith(
        document.getElementById('#b'),
        [],
        {
          header: 'dummyheader',
          div: '#b',
          height: 80,
          width: 100
        }
      );
      expect(window.Dygraph).toHaveBeenCalledWith(
        document.getElementById('#c'),
        [],
        {
          header: 'dummyheader',
          div: '#c',
          height: 80,
          width: 100
        }
      );
    });

    it('updateCharts', function () {
      view.isUpdating = true;
      spyOn(view, 'updateLineChart');
      spyOn(view, 'prepareD3Charts');
      spyOn(view, 'prepareResidentSize');
      spyOn(view, 'updateTendencies');

      view.graphs = {'a': 1, 'b': 2, 'c': 3};
      view.updateCharts();
      expect(view.prepareD3Charts).toHaveBeenCalledWith(true);
      expect(view.prepareResidentSize).toHaveBeenCalledWith(true);
      expect(view.updateTendencies).toHaveBeenCalled();
      expect(view.updateLineChart).toHaveBeenCalledWith('a', false);
      expect(view.updateLineChart).toHaveBeenCalledWith('b', false);
      expect(view.updateLineChart).toHaveBeenCalledWith('c', false);
    });

    it('updateCharts in detail mode', function () {
      view.isUpdating = true;
      spyOn(view, 'updateLineChart');
      spyOn(view, 'prepareD3Charts');
      spyOn(view, 'prepareResidentSize');
      spyOn(view, 'updateTendencies');

      view.detailGraph = '1';
      view.detailGraphFigure = 'abc';
      view.updateCharts();
      expect(view.prepareD3Charts).not.toHaveBeenCalled();
      expect(view.prepareResidentSize).not.toHaveBeenCalled();
      expect(view.updateTendencies).not.toHaveBeenCalled();
      expect(view.updateLineChart).toHaveBeenCalledWith('abc', true);
    });

    it('updateTendencies', function () {
      var jQueryDummy = {
        html: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(jQueryDummy, 'html');

      view.tendencies = {'a': 1, 'b': 2, 'c': 3};
      view.history = {};
      view.history[view.server] = {
        a: [1, 2],
        b: [3, 4],
        c: [5, 6]
      };
      view.updateTendencies();
      expect(window.$).toHaveBeenCalledWith('#a');
      expect(window.$).toHaveBeenCalledWith('#b');
      expect(window.$).toHaveBeenCalledWith('#c');
      expect(jQueryDummy.html).toHaveBeenCalledWith(
        '1<br/><span class="dashboard-figurePer" style="color: green;">+2%</span>'
      );
      expect(jQueryDummy.html).toHaveBeenCalledWith(
        '3<br/><span class="dashboard-figurePer" style="color: green;">+4%</span>'
      );

      expect(jQueryDummy.html).toHaveBeenCalledWith(
        '5<br/><span class="dashboard-figurePer" style="color: green;">+6%</span>'
      );
    });

    it('updateDateWindow for detail chart', function () {
      view.interval = 10;
      expect(view.updateDateWindow({dateWindow_: [100, 1000]}, true)).toEqual([100, 1000]);
    });

    it('updateDateWindow for normal chart', function () {
      view.defaultFrame = 10;
      expect(view.updateDateWindow('aaaa', false)).not.toEqual(undefined);
    });

    it('updateLineChart for normal chart', function () {
      var dyGraphDummy = {
        updateOptions: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(view, 'updateDateWindow').andReturn([0, 100]);
      spyOn(dyGraphDummy, 'updateOptions');
      view.graphs = {'aaaa': dyGraphDummy};
      view.updateLineChart('aaaa', false);
      expect(view.updateDateWindow).toHaveBeenCalledWith(dyGraphDummy, false);
      expect(dyGraphDummy.updateOptions).toHaveBeenCalledWith(
        {
          file: undefined,
          dateWindow: [0, 100]
        }
      );
    });

    it('updateLineChart for detail chart', function () {
      var dyGraphDummy = {
        updateOptions: function () {}
      };
      spyOn(view, 'updateDateWindow').andReturn([0, 100]);
      spyOn(dyGraphDummy, 'updateOptions');
      view.detailGraph = dyGraphDummy;
      view.updateLineChart('aaaa', true);
      expect(view.updateDateWindow).toHaveBeenCalledWith(dyGraphDummy, true);
      expect(dyGraphDummy.updateOptions).toHaveBeenCalledWith(
        {
          file: undefined,
          dateWindow: [0, 100]
        }
      );
    });

    it('mergeDygraphHistory', function () {
      spyOn(dyGraphConfigDummy, 'getDashBoardFigures').andCallThrough();

      view.mergeDygraphHistory({
        times: [1234567, 234567],
        x: ['aa', 'bb'],
        y: [11, 22],
        z: [100, 100]
      }, 0);

      expect(dyGraphConfigDummy.getDashBoardFigures).toHaveBeenCalledWith(true);
      expect(view.history[view.server].a).toEqual([
        [jasmine.any(Date) , 'aa']
      ]);
      expect(view.history[view.server].c).toEqual([
        [jasmine.any(Date) , 100]
      ]);
    });

    it('mergeHistory', function () {
      view.tendencies = {
        virtualSizeAverage: ['y', 'z']
      };
      view.barCharts = {
        barchart: 'bb'
      };
      var param = {
        times: [1234567, 234567],
        x: ['aa', 'bb'],
        y: [11, 22],
        z: [100, 100],
        residentSizePercent: [1, 2],
        nextStart: 'tomorrow'
      };

      spyOn(view, 'mergeDygraphHistory');
      spyOn(view, 'mergeBarChartData');
      view.mergeHistory(param, false);
      expect(view.mergeBarChartData).toHaveBeenCalledWith('bb', param);
      expect(view.mergeDygraphHistory).toHaveBeenCalledWith(param, 0);
      expect(view.mergeDygraphHistory).toHaveBeenCalledWith(param, 1);

      expect(view.nextStart).toEqual('tomorrow');

      expect(view.history[view.server].residentSizeChart).toEqual([
        {
          'key': '',
          'color': dyGraphConfigDummy.colors[1],
          'values': [
            {
              label: 'used',
              value: jasmine.any(Number) // Returns NaN
            }
          ]
        },
        {
          'key': '',
          'color': dyGraphConfigDummy.colors[0],
          'values': [
            {
              label: 'used',
              value: jasmine.any(Number) // Returns NaN
            }
          ]
        }

      ]);
    });

    it('mergeHistory in detail Mode', function () {
      view.tendencies = {
        virtualSizeAverage: ['y', 'z']
      };
      view.barCharts = {
        barchart: 'bb'
      };
      var param = {
        times: [1234567, 234567],
        x: ['aa', 'bb'],
        y: [11, 22],
        z: [100, 100],
        residentSizePercent: [1, 2],
        nextStart: 'tomorrow'
      };

      spyOn(view, 'mergeDygraphHistory');
      spyOn(view, 'mergeBarChartData');
      view.mergeHistory(param);
      expect(view.mergeDygraphHistory).toHaveBeenCalledWith(param, 0);
      expect(view.mergeDygraphHistory).toHaveBeenCalledWith(param, 1);
    });

    it('mergeBarChartData', function () {
      view.barChartsElementNames = {
        b1: 'bb',
        b2: 'bc'
      };
      var v1 = {
          'key': 'bb',
          'color': dyGraphConfigDummy.colors[0],
          'values': [
            {label: 'blub', value: 1},
            {label: 'blub', value: 2}
          ]
        }, v2 = {
          'key': 'bc',
          'color': dyGraphConfigDummy.colors[1],
          'values': [
            {label: 'blub', value: 3},
            {label: 'blub', value: 4}
          ]
      };

      spyOn(view, 'getLabel').andReturn('blub');
      expect(view.mergeBarChartData(['b1', 'b2'],
        {
          b1: {
            cuts: ['cuts'],
            values: [2, 1]
          },
          b2: {
            cuts: ['cuts2'],
            values: [4, 3]
          }
        }
      )
      ).toEqual([v1, v2]);
      expect(view.getLabel).toHaveBeenCalledWith(['cuts'], 0);
      expect(view.getLabel).toHaveBeenCalledWith(['cuts'], 1);
      expect(view.getLabel).toHaveBeenCalledWith(['cuts2'], 0);
      expect(view.getLabel).toHaveBeenCalledWith(['cuts2'], 1);
    });

    it('getLabel with bad counter element', function () {
      expect(view.getLabel([1, 2, 3], 3)).toEqual('>3');
    });
    it('getLabel', function () {
      expect(view.getLabel([1, 2, 3], 2)).toEqual('2 - 3');
    });

    it('getLabel with counter = 0', function () {
      expect(view.getLabel([1, 2, 3], 0)).toEqual('0 - 1');
    });

    it('getStatistics with nextStart', function () {
      view.nextStart = 10000;
      view.server = {
        endpoint: 'abcde',
        target: 'xyz'
      };
      spyOn(view, 'mergeHistory');
      spyOn(view, 'updateTendencies');
      spyOn(view, 'updateLineChart');
      spyOn($, 'ajax').andCallFake(function (url, opt) {
        expect(url).toEqual(
          'abcde/_admin/aardvark/statistics/cluster?start=10000&type=short&DBserver=xyz'
        );
        expect(opt.async).toEqual(true);
        return {
          done: function (y) {
            y({
              times: [1, 2, 3]
            });
          }
        };
      });

      view.getStatistics();
      expect(view.mergeHistory).toHaveBeenCalledWith({
        times: [1, 2, 3]
      });
      expect(view.updateTendencies).toHaveBeenCalled();
      expect(view.isUpdating).toEqual(true);
      Object.keys(view.graphs).forEach(function (f) {
        expect(view.updateLineChart).toHaveBeenCalledWith(f, false);
      });
    });

    it('prepare D3Charts', function () {
      spyOn(nv, 'addGraph').andCallFake(function (a, b) {
        a();
        b();
      });
      spyOn(nv.utils, 'windowResize');
      spyOn(nv.models, 'multiBarHorizontalChart').andReturn(d3ChartDummy);
      spyOn(d3, 'select').andReturn(d3ChartDummy);
      spyOn(view, 'getCurrentSize').andReturn({height: 190, width: 200});

      view.history = {};
      view.history[view.server] = {
        totalTimeDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

        ], dataTransferDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

      ]};
      view.d3NotInitialized = false;
      view.prepareD3Charts(true);

      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#totalTimeDistributionContainer .dashboard-interior-chart');
      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#dataTransferDistributionContainer .dashboard-interior-chart');

      expect(nv.models.multiBarHorizontalChart).toHaveBeenCalled();
    });

    it('prepare D3Charts no update', function () {
      spyOn(nv, 'addGraph').andCallFake(function (a, b) {
        a();
        b();
      });
      spyOn(nv.utils, 'windowResize');
      spyOn(nv.models, 'multiBarHorizontalChart').andReturn(d3ChartDummy);

      spyOn(d3, 'select').andReturn(d3ChartDummy);

      spyOn(view, 'getCurrentSize').andReturn({height: 190, width: 200});

      view.history = {};
      view.history[view.server] = {totalTimeDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

        ], dataTransferDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

      ]};
      view.prepareD3Charts(false);

      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#totalTimeDistributionContainer .dashboard-interior-chart');
      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#dataTransferDistributionContainer .dashboard-interior-chart');

      expect(nv.models.multiBarHorizontalChart).toHaveBeenCalled();
    });

    it('prepare D3Charts no update width > 400', function () {
      spyOn(nv, 'addGraph').andCallFake(function (a, b) {
        a();
        b();
      });
      spyOn(nv.utils, 'windowResize');
      spyOn(nv.models, 'multiBarHorizontalChart').andReturn(d3ChartDummy);

      spyOn(d3, 'select').andReturn(d3ChartDummy);

      spyOn(view, 'getCurrentSize').andReturn({height: 190, width: 404});

      view.history = {};
      view.history[view.server] = {totalTimeDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

        ], dataTransferDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

      ]};
      view.prepareD3Charts(false);

      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#totalTimeDistributionContainer .dashboard-interior-chart');
      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#dataTransferDistributionContainer .dashboard-interior-chart');

      expect(nv.models.multiBarHorizontalChart).toHaveBeenCalled();
    });

    it('prepare D3Charts no update width > 300', function () {
      spyOn(nv, 'addGraph').andCallFake(function (a, b) {
        a();
        b();
      });
      spyOn(nv.utils, 'windowResize');
      spyOn(nv.models, 'multiBarHorizontalChart').andReturn(d3ChartDummy);

      spyOn(d3, 'select').andReturn(d3ChartDummy);

      spyOn(view, 'getCurrentSize').andReturn({height: 190, width: 304});

      view.history = {};
      view.history[view.server] = {
        totalTimeDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

        ], dataTransferDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

      ]};
      view.prepareD3Charts(false);

      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#totalTimeDistributionContainer .dashboard-interior-chart');
      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#dataTransferDistributionContainer .dashboard-interior-chart');

      expect(nv.models.multiBarHorizontalChart).toHaveBeenCalled();
    });

    it('prepare D3Charts no update width > 200', function () {
      spyOn(nv, 'addGraph').andCallFake(function (a, b) {
        a();
        b();
      });
      spyOn(nv.utils, 'windowResize');
      spyOn(nv.models, 'multiBarHorizontalChart').andReturn(d3ChartDummy);

      spyOn(d3, 'select').andReturn(d3ChartDummy);

      spyOn(view, 'getCurrentSize').andReturn({height: 190, width: 204});

      view.history = {};
      view.history[view.server] = {
        totalTimeDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

        ], dataTransferDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

      ]};
      view.prepareD3Charts(false);

      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#totalTimeDistributionContainer .dashboard-interior-chart');
      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#dataTransferDistributionContainer .dashboard-interior-chart');

      expect(nv.models.multiBarHorizontalChart).toHaveBeenCalled();
    });

    it('prepare D3Charts no update width smaller 100', function () {
      spyOn(nv, 'addGraph').andCallFake(function (a, b) {
        a();
        b();
      });
      spyOn(nv.utils, 'windowResize');
      spyOn(nv.models, 'multiBarHorizontalChart').andReturn(d3ChartDummy);

      spyOn(d3, 'select').andReturn(d3ChartDummy);

      spyOn(view, 'getCurrentSize').andReturn({height: 190, width: 11});

      view.history = {};
      view.history[view.server] = {
        totalTimeDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

        ], dataTransferDistribution: [
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[1],
            'values': [
              {
                label: 'used',
                value: 20
              }
            ]
          },
          {
            'key': '',
            'color': dyGraphConfigDummy.colors[0],
            'values': [
              {
                label: 'used',
                value: 80
              }
            ]
          }

      ]};
      view.prepareD3Charts(false);

      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#totalTimeDistributionContainer .dashboard-interior-chart');
      expect(view.getCurrentSize).toHaveBeenCalledWith(
        '#dataTransferDistributionContainer .dashboard-interior-chart');

      expect(nv.models.multiBarHorizontalChart).toHaveBeenCalled();
    });

    it('stopUpdating', function () {
      view.stopUpdating();
      expect(view.isUpdating).toEqual(false);
    });

    it('startUpdating with running timer', function () {
      view.timer = 1234;
      spyOn(window, 'setInterval');
      view.startUpdating();
      expect(window.setInterval).not.toHaveBeenCalled();
    });

    it('startUpdating with no timer but no statistics updates', function () {
      spyOn(view, 'getStatistics');
      spyOn(view, 'updateCharts');
      view.isUpdating = false;
      spyOn(window, 'setInterval').andCallFake(
        function (a) {
          a();
        }
      );
      view.startUpdating();
      expect(window.setInterval).toHaveBeenCalled();
      expect(view.getStatistics).toHaveBeenCalled();
      expect(view.updateCharts).not.toHaveBeenCalled();
    });

    it('startUpdating with no timer and statistics updates', function () {
      spyOn(view, 'getStatistics');
      view.isUpdating = true;
      spyOn(window, 'setInterval').andCallFake(
        function (a) {
          a();
        }
      );
      view.startUpdating();
      expect(window.setInterval).toHaveBeenCalled();
      expect(view.getStatistics).toHaveBeenCalled();
    });

    it('resize', function () {
      spyOn(view, 'getCurrentSize').andReturn({
        width: 100,
        height: 10

      });
      spyOn(view, 'prepareD3Charts');
      spyOn(view, 'prepareResidentSize');

      var dyGraphDummy = {
        resize: function () {},
        maindiv_: {id: 'maindiv'}
      };
      spyOn(dyGraphDummy, 'resize');

      view.graphs = {'aaaa': dyGraphDummy};
      view.isUpdating = true;

      spyOn(window, 'setInterval').andCallFake(
        function (a) {
          a();
        }
      );
      view.resize();
      expect(view.getCurrentSize).toHaveBeenCalledWith('maindiv');
      expect(dyGraphDummy.resize).toHaveBeenCalledWith(100, 10);
      expect(view.prepareD3Charts).toHaveBeenCalledWith(true);
      expect(view.prepareResidentSize).toHaveBeenCalledWith(true);
    });

    it('resize when nothing is updating', function () {
      spyOn(view, 'getCurrentSize').andReturn({
        width: 100,
        height: 10

      });
      spyOn(view, 'prepareD3Charts');
      spyOn(view, 'prepareResidentSize');

      var dyGraphDummy = {
        resize: function () {},
        maindiv_: {id: 'maindiv'}
      };
      spyOn(dyGraphDummy, 'resize');

      view.graphs = {'aaaa': dyGraphDummy};
      view.isUpdating = false;

      spyOn(window, 'setInterval').andCallFake(
        function (a) {
          a();
        }
      );
      view.resize();
      expect(view.getCurrentSize).not.toHaveBeenCalled();
      expect(dyGraphDummy.resize).not.toHaveBeenCalled();
      expect(view.prepareD3Charts).not.toHaveBeenCalled();
      expect(view.prepareResidentSize).not.toHaveBeenCalled();
    });

    it('resize with detail chart', function () {
      spyOn(view, 'getCurrentSize').andReturn({
        width: 100,
        height: 10

      });
      spyOn(view, 'prepareD3Charts');
      spyOn(view, 'prepareResidentSize');

      var dyGraphDummy = {
        resize: function () {},
        maindiv_: {id: 'maindiv'}
      };
      spyOn(dyGraphDummy, 'resize');

      view.graphs = {};
      view.detailGraph = dyGraphDummy;
      view.isUpdating = true;

      spyOn(window, 'setInterval').andCallFake(
        function (a) {
          a();
        }
      );
      view.resize();
      expect(view.getCurrentSize).toHaveBeenCalledWith('maindiv');
      expect(dyGraphDummy.resize).toHaveBeenCalledWith(100, 10);
      expect(view.prepareD3Charts).toHaveBeenCalledWith(true);
      expect(view.prepareResidentSize).toHaveBeenCalledWith(true);
    });

    it('render without modal and no updating', function () {
      var jQueryDummy = {
        html: function () {},
        remove: function () {},
        append: function () {}
      };
      spyOn(view, 'startUpdating');
      spyOn(view, 'getStatistics').andCallFake(function (cb) {
        cb();
      });
      spyOn(view, 'prepareDygraphs');

      spyOn(view, 'prepareD3Charts');
      spyOn(view, 'prepareResidentSize');
      spyOn(view, 'updateTendencies');

      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(jQueryDummy, 'html');
      spyOn(jQueryDummy, 'remove');
      spyOn(jQueryDummy, 'append');
      spyOn(databaseDummy, 'hasSystemAccess').andReturn(true);
      view.isUpdating = false;
      view.render(false);

      expect(window.$).toHaveBeenCalledWith(view.el);

      expect(view.startUpdating).toHaveBeenCalled();
      expect(view.getStatistics).toHaveBeenCalled();
      expect(view.prepareDygraphs).toHaveBeenCalled();

      expect(view.prepareD3Charts).not.toHaveBeenCalled();
      expect(view.prepareResidentSize).not.toHaveBeenCalled();
      expect(view.updateTendencies).not.toHaveBeenCalled();

      expect(jQueryDummy.html).toHaveBeenCalled();
    });

    it('render without modal and updating', function () {
      var jQueryDummy = {
        html: function () {},
        remove: function () {},
        append: function () {}
      };
      spyOn(view, 'startUpdating');
      spyOn(view, 'getStatistics').andCallFake(function (cb) {
        cb();
      });
      spyOn(view, 'prepareDygraphs');

      spyOn(view, 'prepareD3Charts');
      spyOn(view, 'prepareResidentSize');
      spyOn(view, 'updateTendencies');

      spyOn(window, '$').andReturn(
        jQueryDummy
      );
      spyOn(databaseDummy, 'findWhere').andCallFake(function () {
        return true;
      });
      spyOn(jQueryDummy, 'html');
      spyOn(jQueryDummy, 'remove');
      spyOn(jQueryDummy, 'append');
      spyOn(databaseDummy, 'hasSystemAccess').andReturn(true);
      view.isUpdating = true;
      view.render(false);

      expect(window.$).toHaveBeenCalledWith(view.el);
      expect(view.startUpdating).toHaveBeenCalled();
      expect(view.getStatistics).toHaveBeenCalled();
      expect(view.prepareDygraphs).toHaveBeenCalled();

      expect(view.prepareD3Charts).toHaveBeenCalled();
      expect(view.prepareResidentSize).toHaveBeenCalled();
      expect(view.updateTendencies).toHaveBeenCalled();

      expect(jQueryDummy.html).toHaveBeenCalled();
    });
  });
}());
