/* jshint browser: true */
/* jshint unused: false */
/* global $, arangoHelper, jasmine, nv, d3, describe, beforeEach, afterEach, it, spyOn, expect*/

(function () {
  'use strict';

  describe('The Show Cluster View', function () {
    var view, serverDummy, arangoDocumentsDummy, statisticsDescriptionDummy,
      jqueryDummy,serverDummy2, dyGraphConfigDummy;

    beforeEach(function () {
      window.App = {
        navigate: function () {
          throw 'This should be a spy';
        },
        addAuth: {
          bind: function () {
            return 'authBinding';
          }
        },
        serverToShow: undefined,

        dashboard: function () {
          return undefined;
        }

      };

      arangoDocumentsDummy = {
        getStatisticsHistory: function () {
          return undefined;
        },
        history: [
          {
            time: 20000,
            server: {
              uptime: 10000
            },
            client: {
              totalTime: {
                count: 0,
                sum: 0
              }
            }
          },
          {
            time: 30000,
            server: {
              uptime: 100
            },
            client: {
              totalTime: {
                count: 1,
                sum: 4
              }
            }
          }

        ]
      };
      serverDummy = {
        getList: function () {
          return undefined;
        },
        byAddress: function () {
          return undefined;
        },
        getStatuses: function () {
          return undefined;
        },

        findWhere: function () {
          return undefined;
        },
        forEach: function () {
          return undefined;
        }

      };
      serverDummy2 = {
        getList: function () {
          return undefined;
        },
        byAddress: function () {
          return undefined;
        },
        getStatuses: function () {
          return undefined;
        },

        findWhere: function () {
          return undefined;
        },
        forEach: function () {
          return undefined;
        },
        first: function () {
          return undefined;
        }

      };

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
        getColors: function () {
          return undefined;
        },
        colors: [1, 2]

      };

      statisticsDescriptionDummy = {
        fetch: function () {
          return undefined;
        }

      };

      spyOn(window, 'ClusterServers').andReturn(serverDummy);
      spyOn(window, 'ClusterDatabases').andReturn(serverDummy);
      spyOn(window, 'ClusterCoordinators').andReturn(serverDummy2);
      spyOn(window, 'ClusterCollections').andReturn(serverDummy);
      spyOn(window, 'ClusterShards').andReturn(serverDummy);
      spyOn(window, 'arangoDocuments').andReturn(arangoDocumentsDummy);
      spyOn(window, 'StatisticsDescription').andReturn(statisticsDescriptionDummy);

      spyOn(statisticsDescriptionDummy, 'fetch');
      view = new window.ShowClusterView({dygraphConfig: dyGraphConfigDummy});
      expect(view.interval).toEqual(10000);
      expect(view.isUpdating).toEqual(true);
      expect(view.knownServers).toEqual([]);
      expect(view.graph).toEqual(undefined);

      expect(view.graphShowAll).toEqual(false);
      expect(window.ClusterServers).toHaveBeenCalledWith([], {
        interval: view.interval
      });
      expect(window.ClusterCoordinators).toHaveBeenCalledWith([], {
        interval: view.interval
      });
      expect(window.ClusterDatabases).toHaveBeenCalledWith([], {
        interval: view.interval
      });
      expect(statisticsDescriptionDummy.fetch).toHaveBeenCalledWith({
        async: false
      });
    });

    afterEach(function () {
      delete window.App;
    });

    it('assert the basics', function () {
      expect(view.events).toEqual({
        'change #selectDB': 'updateCollections',
        'change #selectCol': 'updateShards',
        'click .dbserver': 'dashboard',
        'click .coordinator': 'dashboard'
      });
      expect(view.defaultFrame).toEqual(20 * 60 * 1000);
    });

    it('assert replaceSVGs', function () {
      spyOn($.fn, 'each').andCallFake(function (a) {
        a();
      });
      spyOn($, 'get').andCallFake(function (a, b) {
        b();
      });

      spyOn($.fn, 'find').andCallThrough();
      view.replaceSVGs();
      expect($.fn.find).toHaveBeenCalledWith('svg');
    });

    it('assert updateServerTime', function () {
      view.serverTime = 10;
      var before = view.serverTime;
      view.updateServerTime();
      expect(view.serverTime > before).toEqual(true);
    });

    it('assert setShowAll', function () {
      view.setShowAll();
      expect(view.graphShowAll).toEqual(true);
    });

    it('assert resetShowAll', function () {
      spyOn(view, 'renderLineChart');
      view.resetShowAll();
      expect(view.graphShowAll).toEqual(false);
      expect(view.renderLineChart).toHaveBeenCalled();
    });

    it('assert listByAddress', function () {
      spyOn(serverDummy2, 'byAddress').andReturn('byAddress');
      spyOn(serverDummy, 'byAddress').andReturn('byAddress');
      view.listByAddress();
      expect(view.graphShowAll).toEqual(false);
      expect(serverDummy.byAddress).toHaveBeenCalledWith();
      expect(serverDummy2.byAddress).toHaveBeenCalledWith('byAddress');
    });

    it('assert updateCollections', function () {
      jqueryDummy = {
        find: function () {
          return jqueryDummy;
        },
        attr: function () {
          return undefined;
        },
        html: function () {
          return jqueryDummy;
        },
        append: function () {
          return jqueryDummy;
        }
      };

      spyOn(window, '$').andReturn(jqueryDummy);
      spyOn(jqueryDummy, 'find').andCallThrough();
      spyOn(jqueryDummy, 'attr').andReturn('dbName');
      spyOn(jqueryDummy, 'html');
      spyOn(jqueryDummy, 'append');

      spyOn(serverDummy, 'getList').andReturn(
        [
          {name: 'a'},
          {name: 'b'},
          {name: 'c'}
        ]

      );

      spyOn(view, 'updateShards');

      view.updateCollections();

      expect(window.$).toHaveBeenCalledWith('#selectDB');
      expect(window.$).toHaveBeenCalledWith('#selectCol');

      expect(view.updateShards).toHaveBeenCalled();

      expect(serverDummy.getList).toHaveBeenCalledWith('dbName');

      expect(jqueryDummy.find).toHaveBeenCalledWith(':selected');
      expect(jqueryDummy.attr).toHaveBeenCalledWith('id');
      expect(jqueryDummy.html).toHaveBeenCalledWith('');
      expect(jqueryDummy.append).toHaveBeenCalledWith(
        '<option id="' + 'c' + '">' + 'c' + '</option>'
      );
      expect(jqueryDummy.append).toHaveBeenCalledWith(
        '<option id="' + 'a' + '">' + 'a' + '</option>'
      );

      expect(jqueryDummy.append).toHaveBeenCalledWith(
        '<option id="' + 'b' + '">' + 'b' + '</option>'
      );
    });

    it('assert updateShards', function () {
      jqueryDummy = {
        find: function () {
          return jqueryDummy;
        },
        attr: function () {
          return undefined;
        },
        html: function () {
          return jqueryDummy;
        },
        append: function () {
          return jqueryDummy;
        }

      };

      spyOn(window, '$').andReturn(jqueryDummy);
      spyOn(jqueryDummy, 'find').andCallThrough();
      spyOn(jqueryDummy, 'attr').andReturn('dbName');
      spyOn(jqueryDummy, 'html');
      spyOn(jqueryDummy, 'append');

      spyOn(serverDummy, 'getList').andReturn(
        [
          {server: 'a', shards: {length: 1}},
          {server: 'b', shards: {length: 2}},
          {server: 'c', shards: {length: 3}}
        ]

      );

      spyOn(serverDummy2, 'getList').andReturn(
        [
          {server: 'a', shards: {length: 1}},
          {server: 'b', shards: {length: 2}},
          {server: 'c', shards: {length: 3}}
        ]

      );

      view.updateShards();

      expect(window.$).toHaveBeenCalledWith('#selectDB');
      expect(window.$).toHaveBeenCalledWith('#selectCol');
      expect(window.$).toHaveBeenCalledWith('.shardCounter');
      expect(window.$).toHaveBeenCalledWith('#aShards');
      expect(window.$).toHaveBeenCalledWith('#bShards');
      expect(window.$).toHaveBeenCalledWith('#cShards');

      expect(serverDummy.getList).toHaveBeenCalledWith('dbName', 'dbName');

      expect(jqueryDummy.find).toHaveBeenCalledWith(':selected');
      expect(jqueryDummy.attr).toHaveBeenCalledWith('id');
      expect(jqueryDummy.html).toHaveBeenCalledWith('0');
      expect(jqueryDummy.html).toHaveBeenCalledWith(1);
      expect(jqueryDummy.html).toHaveBeenCalledWith(2);
      expect(jqueryDummy.html).toHaveBeenCalledWith(3);
    });

    it('assert updateServerStatus', function () {
      jqueryDummy = {
        attr: function () {
          return undefined;
        }
      };
      spyOn(window, '$').andReturn(jqueryDummy);

      spyOn(jqueryDummy, 'attr').andReturn('a s');
      spyOn(serverDummy, 'getStatuses').andCallFake(function (a) {
        a('ok', '123.456.789:10');
      });
      spyOn(serverDummy2, 'getStatuses').andCallFake(function (a) {
        a('ok', '123.456.789:10');
      });
      view.updateServerStatus();
      expect(jqueryDummy.attr).toHaveBeenCalledWith('class');
      expect(jqueryDummy.attr).toHaveBeenCalledWith('class', 'dbserver ' + 's' + ' ' + 'ok');
      expect(jqueryDummy.attr).toHaveBeenCalledWith('class',
        'coordinator ' + 's' + ' ' + 'ok');
      expect(window.$).toHaveBeenCalledWith('#id123-456-789_10');
    });

    it('assert updateDBDetailList', function () {
      jqueryDummy = {
        find: function () {
          return jqueryDummy;
        },
        attr: function () {
          return undefined;
        },
        html: function () {
          return jqueryDummy;
        },
        append: function () {
          return jqueryDummy;
        },
        prop: function () {
          return undefined;
        }
      };

      spyOn(serverDummy, 'getList').andReturn(
        [
          {name: 'a', shards: {length: 1}},
          {name: 'b', shards: {length: 2}},
          {name: 'c', shards: {length: 3}}
        ]

      );

      spyOn(window, '$').andReturn(jqueryDummy);
      spyOn(jqueryDummy, 'find').andCallThrough();
      spyOn(jqueryDummy, 'attr').andReturn('dbName');
      spyOn(jqueryDummy, 'html');
      spyOn(jqueryDummy, 'append');
      spyOn(jqueryDummy, 'prop');

      view.updateDBDetailList();

      expect(window.$).toHaveBeenCalledWith('#selectDB');
      expect(window.$).toHaveBeenCalledWith('#selectCol');

      expect(jqueryDummy.find).toHaveBeenCalledWith(':selected');
      expect(jqueryDummy.prop).toHaveBeenCalledWith('selected', true);
      expect(jqueryDummy.attr).toHaveBeenCalledWith('id');
      expect(jqueryDummy.html).toHaveBeenCalledWith('');
      expect(jqueryDummy.append).toHaveBeenCalledWith(
        '<option id="' + 'c' + '">' + 'c' + '</option>'
      );
      expect(jqueryDummy.append).toHaveBeenCalledWith(
        '<option id="' + 'a' + '">' + 'a' + '</option>'
      );

      expect(jqueryDummy.append).toHaveBeenCalledWith(
        '<option id="' + 'b' + '">' + 'b' + '</option>'
      );
    });

    it('assert updateDBDetailList with selected db', function () {
      jqueryDummy = {
        find: function () {
          return jqueryDummy;
        },
        attr: function () {
          return undefined;
        },
        html: function () {
          return jqueryDummy;
        },
        append: function () {
          return jqueryDummy;
        },
        prop: function () {
          return undefined;
        },
        length: 1
      };

      spyOn(serverDummy, 'getList').andReturn(
        [
          {name: 'a', shards: {length: 1}},
          {name: 'b', shards: {length: 2}},
          {name: 'c', shards: {length: 3}}
        ]

      );

      spyOn(window, '$').andReturn(jqueryDummy);
      spyOn(jqueryDummy, 'find').andCallThrough();
      spyOn(jqueryDummy, 'attr').andReturn('dbName');
      spyOn(jqueryDummy, 'html');
      spyOn(jqueryDummy, 'append');
      spyOn(jqueryDummy, 'prop');

      view.updateDBDetailList();

      expect(window.$).toHaveBeenCalledWith('#selectDB');
      expect(window.$).toHaveBeenCalledWith('#selectCol');

      expect(jqueryDummy.find).toHaveBeenCalledWith(':selected');
      expect(jqueryDummy.prop).toHaveBeenCalledWith('selected', true);
      expect(jqueryDummy.attr).toHaveBeenCalledWith('id');
      expect(jqueryDummy.html).toHaveBeenCalledWith('');
      expect(jqueryDummy.append).toHaveBeenCalledWith(
        '<option id="' + 'c' + '">' + 'c' + '</option>'
      );
      expect(jqueryDummy.append).toHaveBeenCalledWith(
        '<option id="' + 'a' + '">' + 'a' + '</option>'
      );

      expect(jqueryDummy.append).toHaveBeenCalledWith(
        '<option id="' + 'b' + '">' + 'b' + '</option>'
      );
    });

    it('assert rerender', function () {
      spyOn(view, 'updateServerStatus');
      spyOn(view, 'getServerStatistics');
      spyOn(view, 'updateServerTime');
      spyOn(view, 'generatePieData').andReturn({data: '1'});
      spyOn(view, 'renderPieChart');
      spyOn(view, 'renderLineChart');
      spyOn(view, 'updateDBDetailList');
      view.rerender();
      expect(view.updateServerStatus).toHaveBeenCalled();
      expect(view.getServerStatistics).toHaveBeenCalled();
      expect(view.updateServerTime).toHaveBeenCalled();
      expect(view.generatePieData).toHaveBeenCalled();
      expect(view.renderPieChart).toHaveBeenCalledWith({data: '1'});
      expect(view.renderLineChart).toHaveBeenCalled();
      expect(view.updateDBDetailList).toHaveBeenCalled();
    });

    it('assert render', function () {
      spyOn(view, 'startUpdating');
      spyOn(view, 'replaceSVGs');
      spyOn(view, 'listByAddress').andReturn({data: '1'});
      spyOn(view, 'generatePieData').andReturn({data: '1'});
      spyOn(view, 'getServerStatistics');
      spyOn(view, 'renderPieChart');
      spyOn(view, 'renderLineChart');
      spyOn(view, 'updateCollections');
      view.template = {
        render: function () {
          return undefined;
        }
      };
      spyOn(serverDummy, 'getList').andReturn([
        {name: 'a', shards: {length: 1}},
        {name: 'b', shards: {length: 2}},
        {name: 'c', shards: {length: 3}}
      ]);
      spyOn(view.template, 'render');
      view.render();
      expect(view.startUpdating).toHaveBeenCalled();
      expect(view.listByAddress).toHaveBeenCalled();
      expect(view.replaceSVGs).toHaveBeenCalled();
      expect(view.getServerStatistics).toHaveBeenCalled();
      expect(view.renderPieChart).toHaveBeenCalledWith({data: '1'});
      expect(view.template.render).toHaveBeenCalledWith({
        dbs: ['a', 'b', 'c'],
        byAddress: {data: '1'},
        type: 'testPlan'
      });
      expect(view.generatePieData).toHaveBeenCalled();
      expect(view.renderLineChart).toHaveBeenCalled();
      expect(view.updateCollections).toHaveBeenCalled();
    });

    it('assert render with more than one address', function () {
      spyOn(view, 'startUpdating');
      spyOn(view, 'replaceSVGs');
      spyOn(view, 'listByAddress').andReturn({data: '1', 'ss': 1});
      spyOn(view, 'generatePieData').andReturn({data: '1'});
      spyOn(view, 'getServerStatistics');
      spyOn(view, 'renderPieChart');
      spyOn(view, 'renderLineChart');
      spyOn(view, 'updateCollections');
      view.template = {
        render: function () {
          return undefined;
        }
      };
      spyOn(serverDummy, 'getList').andReturn([
        {name: 'a', shards: {length: 1}},
        {name: 'b', shards: {length: 2}},
        {name: 'c', shards: {length: 3}}
      ]);
      spyOn(view.template, 'render');
      view.render();
      expect(view.startUpdating).toHaveBeenCalled();
      expect(view.listByAddress).toHaveBeenCalled();
      expect(view.replaceSVGs).toHaveBeenCalled();
      expect(view.getServerStatistics).toHaveBeenCalled();
      expect(view.renderPieChart).toHaveBeenCalledWith({data: '1'});
      expect(view.template.render).toHaveBeenCalledWith({
        dbs: ['a', 'b', 'c'],
        byAddress: {data: '1', 'ss': 1},
        type: 'other'
      });
      expect(view.generatePieData).toHaveBeenCalled();
      expect(view.renderLineChart).toHaveBeenCalled();
      expect(view.updateCollections).toHaveBeenCalled();
    });

    it('assert generatePieData', function () {
      view.data = [
        {
          get: function (a) {
            if (a === 'name') {
              return 'name1';
            }
            return {virtualSize: 'residentsize1'};
          }
        },
        {
          get: function (a) {
            if (a === 'name') {
              return 'name2';
            }
            return {virtualSize: 'residentsize2'};
          }
        }
      ];

      expect(view.generatePieData()).toEqual([
        {
          key: 'name1',
          value: 'residentsize1',
          time: view.serverTime
        },
        {
          key: 'name2',
          value: 'residentsize2',
          time: view.serverTime
        }
      ]);
    });

    /*
     it("assert loadHistory", function () {
       var serverResult = [
         {
           id : 1,
           get : function (a) {
             if (a === "protocol") {return  "http";}
             if (a === "address") {return  "123.456.789";}
             if (a === "status") {return  "ok";}
             if (a === "name") {return  "heinz";}
           }
         },
         {
           id : 2,
           get : function (a) {
             if (a === "protocol") {return  "https";}
             if (a === "address") {return  "123.456.799";}
             if (a === "status") {return  "ok";}
             if (a === "name") {return  "herbert";}
           }
         },
         {
           id : 3,
           get : function (a) {
             if (a === "protocol") {return  "http";}
             if (a === "address") {return  "123.456.119";}
             if (a === "status") {return  "notOk";}
             if (a === "name") {return  "heinzle";}
           }
         }
       ], serverResult2 = [
         {
           id : 4,
           get : function (a) {
             if (a === "protocol") {return  "http";}
             if (a === "address") {return  "123.456.789";}
             if (a === "status") {return  "ok";}
             if (a === "name") {return  "heinz";}
           }
         },
         {
           id : 5,
           get : function (a) {
             if (a === "protocol") {return  "https";}
             if (a === "address") {return  "123.456.799";}
             if (a === "status") {return  "ok";}
             if (a === "name") {return  "herbert";}
           }
         },
         {
           id : 6,
           get : function (a) {
             if (a === "protocol") {return  "http";}
             if (a === "address") {return  "123.456.119";}
             if (a === "status") {return  "notOk";}
             if (a === "name") {return  "heinzle";}
           }
         }
       ]

       spyOn(serverDummy2, "findWhere").andReturn(serverResult[0])

       view.dbservers = serverResult
       spyOn(serverDummy, "forEach").andCallFake(function (a) {
         serverResult.forEach(a)
       })
       spyOn(serverDummy2, "forEach").andCallFake(function (a) {
         serverResult2.forEach(a)
       })
       spyOn(arangoDocumentsDummy, "getStatisticsHistory")
       view.loadHistory()

       expect(serverDummy2.findWhere).toHaveBeenCalledWith({
         status: "ok"
       })
       expect(arangoDocumentsDummy.getStatisticsHistory).toHaveBeenCalledWith({
         server: {
           raw: "123.456.789",
           isDBServer: true,
           target: "heinz",
           endpoint: "http://123.456.789",
           addAuth: "authBinding"
         },
         figures: ["client.totalTime"]
       })
       expect(arangoDocumentsDummy.getStatisticsHistory).toHaveBeenCalledWith({
         server: {
           raw: "123.456.799",
           isDBServer: true,
           target:  "herbert",
           endpoint: "http://123.456.789",
           addAuth: "authBinding"
         },
         figures: ["client.totalTime"]
       })
       expect(arangoDocumentsDummy.getStatisticsHistory).not.toHaveBeenCalledWith({
         server: {
           raw: "123.456.119",
           isDBServer: true,
           target:  "heinzle",
           endpoint: "http://123.456.789",
           addAuth: "authBinding"
         },
         figures: ["client.totalTime"]
       })
       expect(view.hist).toEqual({
         1: {lastTime: 30000000, 20000000: 0, 25000000: null, 30000000: 4},
         2: {lastTime: 30000000, 20000000: 0, 25000000: null, 30000000: 4},
         4: {lastTime: 30000000, 20000000: 0, 25000000: null, 30000000: 4},
         5: {lastTime: 30000000, 20000000: 0, 25000000: null, 30000000: 4}}
     )

   })
   */

    it('assert getServerStatistics', function () {
      var getServer1 = function (a) {
          switch (a) {
            case 'protocol':
              return 'http';
            case 'address':
              return '123.456.789';
            case 'status':
              return 'ok';
            case 'name':
              return 'heinz';
            default:
              throw 'Requested unknown attribute: ' + a;
          }
        },
        getServer2 = function (a) {
          switch (a) {
            case 'protocol':
              return 'https';
            case 'address':
              return '123.456.799';
            case 'status':
              return 'ok';
            case 'name':
              return 'herbert';
            default:
              throw 'Requested unknown attribute: ' + a;
          }
        },
        getServer3 = function (a) {
          switch (a) {
            case 'protocol':
              return 'http';
            case 'address':
              return '123.456.119';
            case 'status':
              return 'notOk';
            case 'name':
              return 'heinzle';
            default:
              throw 'Requested unknown attribute: ' + a;
          }
        },

        clusterStatisticsCollectionDummy = {
          add: function () {
            return undefined;
          },
          fetch: function () {
            return undefined;
          },
          forEach: function () {
            return undefined;
          }
        }, serverResult = [
          {
            id: 1,
            get: getServer1
          },
          {
            id: 2,
            get: getServer2
          },
          {
            id: 3,
            get: getServer3
          }
        ], serverResult2 = [
          {
            id: 4,
            get: getServer1
          },
          {
            id: 5,
            get: getServer2
          },
          {
            id: 6,
            get: getServer3
          }
        ], clusterStatistics = [
          {
            get: function (a) {
              if (a === 'server') {return {
                  uptime: 100
                };}
              if (a === 'client') {return {
                  totalTime: {
                    count: 0,
                    sum: 10
                  }
                };}
              if (a === 'name') {return 'herbert';}
              if (a === 'http') {
                return {
                  requestsTotal: 200
                };
              }
              if (a === 'time') {
                return 20;
              }
            }
          },

          {
            get: function (a) {
              if (a === 'server') {return {
                  uptime: 10
                };}
              if (a === 'client') {return {
                  totalTime: {
                    count: 1,
                    sum: 10
                  }
                };}
              if (a === 'name') {return 'heinz';}
              if (a === 'http') {
                return {
                  requestsTotal: 500
                };
              }
              if (a === 'time') {
                return 30;
              }
            }
          }
        ], foundUrls = [];

      spyOn(window, 'ClusterStatisticsCollection').andReturn(
        clusterStatisticsCollectionDummy
      );
      spyOn(window, 'Statistics').andReturn({});
      spyOn(serverDummy2, 'forEach').andCallFake(function (a) {
        serverResult2.forEach(a);
      });
      spyOn(clusterStatisticsCollectionDummy, 'add').andCallFake(function (y) {
        foundUrls.push(y.url);
      });
      spyOn(clusterStatisticsCollectionDummy, 'fetch');
      spyOn(clusterStatisticsCollectionDummy, 'forEach').andCallFake(function (a) {
        clusterStatistics.forEach(a);
      });

      spyOn(serverDummy2, 'first').andReturn({
        id: 1,
        get: getServer1
      });
      view.hist = {

      };
      view.hist.heinz = [];
      view.hist.herbert = [];
      view.dbservers = serverResult;
      view.getServerStatistics();
      expect(serverDummy2.first).toHaveBeenCalled();
      expect(foundUrls.indexOf(
        'http://123.456.789/_admin/clusterStatistics?DBserver=heinz'
      )).not.toEqual(-1);
      expect(foundUrls.indexOf(
        'http://123.456.789/_admin/clusterStatistics?DBserver=herbert'
      )).not.toEqual(-1);
      expect(foundUrls.indexOf(
        'http://123.456.789/_admin/clusterStatistics?DBserver=heinzle'
      )).toEqual(-1);

      expect(foundUrls.indexOf('http://123.456.789/_admin/statistics')).not.toEqual(-1);
      expect(foundUrls.indexOf('https://123.456.799/_admin/statistics')).not.toEqual(-1);
      expect(foundUrls.indexOf('http://123.456.119/_admin/statistics')).toEqual(-1);
    });

    it('assert renderPieChart', function () {
      var d3Dummy = {
        scale: {
          category20: function () {
            return undefined;
          }
        },
        svg: {
          arc: function () {
            return d3Dummy.svg;
          },
          outerRadius: function () {
            return d3Dummy.svg;
          },
          innerRadius: function () {
            return {
              centroid: function (a) {
                return a;
              }
            };
          }
        },
        layout: {
          pie: function () {
            return d3Dummy.layout;
          },
          sort: function (a) {
            expect(a({value: 1})).toEqual(1);
            return d3Dummy.layout;
          },
          value: function (a) {
            expect(a({value: 1})).toEqual(1);
            return d3Dummy.layout.pie;
          }
        },
        select: function () {
          return d3Dummy;
        },
        remove: function () {
          return undefined;
        },
        append: function () {
          return d3Dummy;
        },
        attr: function (a, b) {
          if (a === 'transform' && typeof b === 'function') {
            expect(b(1)).toEqual('translate(1)');
          }
          return d3Dummy;
        },
        selectAll: function () {
          return d3Dummy;
        },
        data: function () {
          return d3Dummy;
        },
        enter: function () {
          return d3Dummy;
        },
        style: function (a, b) {
          if (typeof b === 'function') {
            expect(b('item', 1)).toEqual(2);
          }
          return d3Dummy;
        },
        text: function (a) {
          a({data: {
              key: '1',
              value: 2
          }});
        }
      };

      spyOn(d3.scale, 'category20').andReturn(function (i) {
        var l = ['red', 'white'];
        return l[i];
      });
      spyOn(d3.svg, 'arc').andReturn(d3Dummy.svg.arc());
      spyOn(d3.layout, 'pie').andReturn(d3Dummy.layout.pie());
      spyOn(d3, 'select').andReturn(d3Dummy.select());
      view.renderPieChart();
    });

    it('assert renderLineChart no remake', function () {
      spyOn(dyGraphConfigDummy, 'getDefaultConfig').andCallThrough();

      view.hist = {
        server1: {
          lastTime: 12345,
          2334500: 1,
          2334600: 2,
          2334700: 3
        },
        server2: {
          lastTime: 12345,
          2334500: 2,
          2334800: 2,
          2334900: 5
        }
      };

      var dygraphDummy = {
      };

      spyOn(window, 'Dygraph').andReturn(dygraphDummy);

      view.renderLineChart(false);

      expect(window.Dygraph).toHaveBeenCalledWith(
        document.getElementById('lineGraph'),
        [],
        {
          header: 'dummyheader',
          labels: [ 'datetime' ],
          div: '#clusterRequestsPerSecond',
          labelsDiv: undefined
        }
      );

      expect(dyGraphConfigDummy.getDefaultConfig).toHaveBeenCalledWith(
        'clusterRequestsPerSecond'
      );
    });

    it('assert renderLineChart with remake', function () {
      spyOn(dyGraphConfigDummy, 'getDefaultConfig').andCallThrough();
      spyOn(dyGraphConfigDummy, 'getColors');

      view.hist = {
        server1: {
          lastTime: 12345,
          2334500: 1,
          2334600: 2,
          2334700: 3
        },
        server2: {
          lastTime: 12345,
          2334500: 2,
          2334800: 2,
          2334900: 5
        }

      };

      var dygraphDummy = {
        setSelection: function () {
          return undefined;
        }
      };

      spyOn(window, 'Dygraph').andReturn(dygraphDummy);

      view.renderLineChart(true);

      expect(window.Dygraph).toHaveBeenCalledWith(
        document.getElementById('lineGraph'),
        [],
        {
          header: 'dummyheader',
          labels: [ 'datetime' ],
          div: '#clusterRequestsPerSecond',
          labelsDiv: undefined
        }
      );

      expect(dyGraphConfigDummy.getDefaultConfig).toHaveBeenCalledWith(
        'clusterRequestsPerSecond'
      );
    });

    /*
    it("assert renderLineChart with detailView", function () {

      spyOn(dyGraphConfigDummy, "getDefaultConfig").andCallThrough()
      spyOn(dyGraphConfigDummy, "getColors")

      view.hist = {
        server1 : {
          lastTime : 12345,
          2334500 : 1,
          2334600 : 2,
          2334700 : 3
        },
        server2 : {
          lastTime : 12345,
          2334500 : 2,
          2334800 : 2,
          2334900 : 5
        }

      }

      var dygraphDummy = {
        setSelection : function () {
          return undefined
        },
        updateOptions : function () {
          return undefined
        }
      }

      spyOn(window, "Dygraph").andReturn(dygraphDummy)
      spyOn(dygraphDummy, "setSelection")
      spyOn(dygraphDummy, "updateOptions").andCallFake(function (opt) {
        expect(opt.labels).toEqual(['Date', 'ClusterAverage (avg)',
          'server1', 'server2 (max)'])
        expect(opt.visibility).toEqual([true, false, true])
        expect(opt.dateWindow).not.toEqual(undefined)
      })

      view.renderLineChart(false)
      view.renderLineChart(false)

      expect(dygraphDummy.setSelection).toHaveBeenCalledWith(
        false, 'ClusterAverage', true
      )
      expect(dygraphDummy.updateOptions).toHaveBeenCalled()

      expect(window.Dygraph).toHaveBeenCalledWith(
        document.getElementById('lineGraph'),
        view.graph.data,
        {
          header : 'dummyheader',
          div : '#clusterAverageRequestTime',
          visibility : [ true, false, true ],
          labels : [ 'Date', 'ClusterAverage (avg)', 'server1', 'server2 (max)' ],
          colors : undefined
        }
      )

      expect(dyGraphConfigDummy.getDefaultConfig).toHaveBeenCalledWith(
        "clusterAverageRequestTime"
      )
      expect(dyGraphConfigDummy.getColors).toHaveBeenCalledWith(
        ['Date', 'ClusterAverage (avg)', 'server1', 'server2 (max)']
      )
      expect(view.chartData.labelsNormal).toEqual(
        ['Date', 'ClusterAverage (avg)', 'server1', 'server2 (max)']
      )
      expect(view.chartData.labelsShowAll).toEqual(
        ['Date', 'ClusterAverage', 'server1', 'server2']
      )
      expect(view.chartData.visibilityNormal).toEqual(
        [true, false, true]
      )
      expect(view.chartData.visibilityShowAll).toEqual(
        [true, true, true]
      )
    })
    */

    /*
     it("assert renderLineChart with detailView and showAll", function () {

       spyOn(dyGraphConfigDummy, "getDefaultConfig").andCallThrough()
       spyOn(dyGraphConfigDummy, "getColors")

       view.hist = {
         server1 : {
           lastTime : 12345,
           2334500 : 1,
           2334600 : 2,
           2334700 : 3
         },
         server2 : {
           lastTime : 12345,
           2334500 : 2,
           2334800 : 2,
           2334900 : 5
         }

       }

       var dygraphDummy = {
         setSelection : function () {
           return undefined
         },
         updateOptions : function () {
           return undefined
         },
         dateWindow_ : [0, 1]
       }

       spyOn(window, "Dygraph").andReturn(dygraphDummy)
       spyOn(dygraphDummy, "setSelection")
       spyOn(dygraphDummy, "updateOptions").andCallFake(function (opt) {
         expect(opt.labels).toEqual(['Date', 'ClusterAverage',
           'server1', 'server2'])
         expect(opt.visibility).toEqual([true, true, true])
         expect(opt.dateWindow).not.toEqual(undefined)
       })

       view.renderLineChart(false)
       view.graphShowAll = true
       view.renderLineChart(false)

       expect(dygraphDummy.setSelection).toHaveBeenCalledWith(
         false, 'ClusterAverage', true
       )
       expect(dygraphDummy.updateOptions).toHaveBeenCalled()

       expect(window.Dygraph).toHaveBeenCalledWith(
         document.getElementById('lineGraph'),
         view.graph.data,
         {
           header : 'dummyheader',
           div : '#clusterAverageRequestTime',
           visibility : [ true, false, true ],
           labels : [ 'Date', 'ClusterAverage (avg)', 'server1', 'server2 (max)' ],
           colors : undefined
         }
       )

       expect(dyGraphConfigDummy.getDefaultConfig).toHaveBeenCalledWith(
         "clusterAverageRequestTime"
       )
       expect(dyGraphConfigDummy.getColors).toHaveBeenCalledWith(
         ['Date', 'ClusterAverage (avg)', 'server1', 'server2 (max)']
       )
       expect(view.chartData.labelsNormal).toEqual(
         ['Date', 'ClusterAverage (avg)', 'server1', 'server2 (max)']
       )
       expect(view.chartData.labelsShowAll).toEqual(
         ['Date', 'ClusterAverage', 'server1', 'server2']
       )
       expect(view.chartData.visibilityNormal).toEqual(
         [true, false, true]
       )
       expect(view.chartData.visibilityShowAll).toEqual(
         [true, true, true]
       )

     })
     */

    it('assert stopUpdating', function () {
      spyOn(window, 'clearTimeout');

      view.stopUpdating();

      expect(window.clearTimeout).toHaveBeenCalledWith(view.timer);
      expect(view.isUpdating).toEqual(false);
    });

    it('assert startUpdating but is already updating', function () {
      spyOn(window, 'setInterval');
      view.isUpdating = true;
      view.startUpdating();

      expect(window.setInterval).not.toHaveBeenCalled();
    });

    it('assert startUpdating', function () {
      spyOn(window, 'setInterval').andCallFake(function (a) {
        a();
      });
      spyOn(view, 'rerender');
      view.isUpdating = false;
      view.startUpdating();
      expect(view.rerender).toHaveBeenCalled();
      expect(window.setInterval).toHaveBeenCalledWith(jasmine.any(Function), view.interval);
    });

    it('assert dashboard for dbserver', function () {
      spyOn(view, 'stopUpdating');
      spyOn(window, '$').andReturn({
        remove: function () {
          return undefined;
        },
        hasClass: function () {
          return true;
        },
        attr: function () {
          return 'AA123-456-789_8529';
        }
      });
      spyOn(serverDummy, 'findWhere').andReturn({
        get: function () {
          return 'name';
        }

      });
      spyOn(serverDummy2, 'findWhere').andReturn({
        get: function (a) {
          if (a === 'protocol') {
            return 'http';
          }
          if (a === 'address') {
            return 'localhost';
          }
          return 'name';
        }
      });
      view.dashboard({currentTarget: 'target'});

      expect(view.stopUpdating).toHaveBeenCalled();
      expect(serverDummy.findWhere).toHaveBeenCalledWith({
        address: '123.456.789:8529'
      });
      expect(serverDummy2.findWhere).toHaveBeenCalledWith({
        status: 'ok'
      });
      expect(window.App.serverToShow).toEqual({
        raw: '123.456.789:8529',
        isDBServer: true,
        endpoint: 'http://localhost',
        target: 'name'
      });
    });

    it('assert dashboard for no dbserver', function () {
      spyOn(view, 'stopUpdating');
      spyOn(window, '$').andReturn({
        remove: function () {
          return undefined;
        },
        hasClass: function () {
          return false;
        },
        attr: function () {
          return 'AA123-456-789_8529';
        }
      });
      spyOn(serverDummy, 'findWhere').andReturn({
        get: function () {
          return 'name';
        }

      });
      spyOn(serverDummy2, 'findWhere').andReturn({
        get: function (a) {
          if (a === 'protocol') {
            return 'http';
          }
          if (a === 'address') {
            return 'localhost';
          }
          return 'name';
        }
      });
      view.dashboard({currentTarget: 'target'});

      expect(view.stopUpdating).toHaveBeenCalled();
      expect(serverDummy2.findWhere).toHaveBeenCalledWith({
        address: '123.456.789:8529'
      });
      expect(window.App.serverToShow).toEqual({
        raw: '123.456.789:8529',
        isDBServer: false,
        endpoint: 'http://localhost',
        target: 'name'

      });
    });

    /*

    it("assert showDetail", function () {
      window.modalView = {
        hide : function () {
          return undefined
        },
        show : function () {
          return undefined
        }
      }

      spyOn(window.modalView, "hide")
      spyOn(window.modalView, "show")

      jqueryDummy = {
        on : function () {
          return undefined
        },
        toggleClass : function () {
          return undefined
        }
      }

      spyOn(window, "$").andReturn(jqueryDummy)

      spyOn(jqueryDummy, "on").andCallFake(function (a, b) {
        expect(a).toEqual("hidden")
        b()
      })
      spyOn(jqueryDummy, "toggleClass")

      spyOn(view, "resetShowAll")

      spyOn(view, "setShowAll")
      spyOn(view, "renderLineChart")

      view.showDetail()

      expect(view.resetShowAll).toHaveBeenCalled()
      expect(view.setShowAll).toHaveBeenCalled()
      expect(view.renderLineChart).toHaveBeenCalledWith(true)

      expect(window.modalView.hide).toHaveBeenCalled()
      expect(window.modalView.show).toHaveBeenCalledWith(
        "modalGraph.ejs",
        "Average request time in milliseconds",
        undefined,
        undefined,
        undefined
      )

      expect(window.$).toHaveBeenCalledWith('#modal-dialog')

      expect(jqueryDummy.toggleClass).toHaveBeenCalledWith(
        "modal-chart-detail", true
      )

    })

    */

    it('assert getCurrentSize', function () {
      jqueryDummy = {
        attr: function () {
          return undefined;
        },
        height: function () {
          return undefined;
        },
        width: function () {
          return undefined;
        }
      };

      spyOn(window, '$').andReturn(jqueryDummy);

      spyOn(jqueryDummy, 'attr');
      spyOn(jqueryDummy, 'height').andReturn(1);
      spyOn(jqueryDummy, 'width').andReturn(2);

      expect(view.getCurrentSize('aDiv')).toEqual({
        height: 1,
        width: 2
      });

      expect(jqueryDummy.attr).toHaveBeenCalledWith(
        'style', ''
      );
      expect(jqueryDummy.height).toHaveBeenCalled();
      expect(jqueryDummy.width).toHaveBeenCalled();
    });

    it('assert resize', function () {
      view.graph = {
        maindiv_: {
          id: 1
        },
        resize: function () {
          return undefined;
        }
      };

      spyOn(view.graph, 'resize');
      spyOn(view, 'getCurrentSize').andReturn({
        height: 1,
        width: 2
      });

      view.resize();
      expect(view.getCurrentSize).toHaveBeenCalledWith(1);
      expect(view.graph.resize).toHaveBeenCalledWith(2, 1);
    });
  });
}());
