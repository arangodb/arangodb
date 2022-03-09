/* jshint browser: true */
/* jshint unused: false */
/* global _, Backbone, btoa, templateEngine, $, window, randomColor, arangoHelper, nv, d3 */
(function () {
  'use strict';

  window.ReplicationView = Backbone.View.extend({
    el: '#content',

    // Modes:
    //  0: No active replication found.
    //  1: Replication per Database found.
    //  2: Replication per Server found.
    //  3: Active-Failover replication found.
    mode: null,

    interval: 5000, // refresh interval
    keepEntries: 100, // keep n-entries (history)

    charts: {},
    nvchartsInit: false,

    loggerGraphsData: [],

    // general info object, valid in all modes
    info: {
      state: null,
      mode: null,
      level: null,
      role: null,
      health: 'Good',
      msg: 'No issues detected.'
    },

    // nodes info object, valid in active failover mode (3)
    nodes: {
      leader: null,
      followers: []
    },

    initialize: function (options) {
      var self = this;

      // start polling with interval
      window.setInterval(function () {
        // fetch the replication state
        self.getStateData();
      }, this.interval);
    },

    template: templateEngine.createTemplate('replicationView.ejs'),

    events: {
      'click #nodes-followers-id span': 'goToApplier',
      'click #repl-follower-table tr': 'goToApplierFromTable'
    },

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    render: function () {
      this.undelegateEvents();
      this.getMode(this.continueRender.bind(this));
    },

    continueRender: function () {
      // mode will be set now
      this.$el.html(this.template.render({
        mode: this.mode,
        info: this.info,
        nodes: this.nodes
      }));

      this.getStateData();
      this.delegateEvents();
    },

    renderStatisticBox: function (name, value, title, rowCount) {
      // box already rendered, just update value
      if ($('#replication-info #nodeattribute-' + name).length) {
        $('#replication-info #nodeattribute-' + name).html(value);
      } else {
        var elem = '';
        if (rowCount === 6) {
          elem += '<div class="pure-u-1-2 pure-u-md-1-3 pure-u-lg-1-6" style="background-color: #fff">';
        } else {
          elem += '<div class="pure-u-1-2 pure-u-md-1-4" style="background-color: #fff">';
        }
        elem += '<div class="valueWrapper">';
        if (title) {
          elem += '<div id="nodeattribute-' + name + '" class="value tippy" title="' + value + '">' + value + '</div>';
        } else {
          elem += '<div id="nodeattribute-' + name + '" class="value">' + value + '</div>';
        }
        elem += '<div class="graphLabel">' + name + '</div>';
        elem += '</div>';
        elem += '</div>';
        $('#replication-info').append(elem);
      }
    },

    goToApplier: function (e) {
      // always system (global applier)
      var endpoint = btoa($(e.currentTarget).attr('data'));
      window.App.navigate('#replication/applier/' + endpoint + '/' + btoa('_system'), {trigger: true});
    },

    goToApplierFromTable: function (e) {
      // per db (single applier)
      var endpoint = btoa(window.location.origin);
      var db = btoa($(e.currentTarget).find('#applier-database-id').html());
      var running = $(e.currentTarget).find('#applier-running-id').html();
      if (running === 'true' || running === true) {
        window.App.navigate('#replication/applier/' + endpoint + '/' + db, {trigger: true});
      } else {
        arangoHelper.arangoMessage('Replication', 'This applier is not running.');
      }
    },

    getActiveFailoverEndpoints: function () {
      var self = this;
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/cluster/endpoints'),
        contentType: 'application/json',
        success: function (data) {
          if (data.endpoints) {
            self.renderEndpoints(data.endpoints);
          } else {
            self.renderEndpoints();
          }
        },
        error: function () {
          self.renderEndpoints();
        }
      });
    },

    renderEndpoints: function (endpoints) {
      var self = this;

      if (endpoints) {
        var leader = endpoints[0];
        var followers = endpoints.slice(1, endpoints.length);

        $('#nodes-leader-id').html(leader.endpoint);
        $('#nodes-followers-id').html('');
        _.each(followers, function (follower) {
          $('#nodes-followers-id').append('<span data="' + self.parseEndpoint(follower.endpoint, true) + '">' + follower.endpoint + '</span>');
        });
      } else {
        $('#nodes-leader-id').html('Error');
        $('#nodes-followers-id').html('Error');
      }
    },

    parseEndpoint: function (endpoint, url) {
      var parsedEndpoint;
      if (endpoint.slice(6, 11) === '[::1]') {
        parsedEndpoint = window.location.host.split(':')[0] + ':' + endpoint.split(':')[4];
      } else if (endpoint.slice(0, 6) === 'tcp://') {
        parsedEndpoint = 'http://' + endpoint.slice(6, endpoint.length);
      } else if (endpoint.slice(0, 6) === 'ssl://') {
        parsedEndpoint = 'https://' + endpoint.slice(6, endpoint.length);
      }

      if (url) {
        parsedEndpoint = window.location.protocol + '//' + parsedEndpoint;
      }

      if (!parsedEndpoint) {
        return endpoint;
      }

      return parsedEndpoint;
    },

    getLoggerState: function () {
      var self = this;
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/replication/logger-state'),
        contentType: 'application/json',
        success: function (data) {
          if (window.location.hash === '#replication') {
            self.updateLoggerGraphsData(data);
            if (!self.nvchartsInit) {
              self.initLoggerGraphs();
            } else {
              self.rerenderLoggerGraphs();
            }
            self.renderLoggerState(data.server, data.clients, data.state);
          } else {
            // update values
            self.updateLoggerGraphsData(data);
          }
        },
        error: function () {
          arangoHelper.arangoError('Replication', 'Could not fetch the leaders logger state.');
        }
      });
    },

    getApplierStates: function (global) {
      var self = this;

      var url;
      if (global) {
        url = arangoHelper.databaseUrl('/_api/replication/applier-state?global=true');
      } else {
        url = arangoHelper.databaseUrl('/_api/replication/applier-state-all');
      }

      $.ajax({
        type: 'GET',
        cache: false,
        url: url,
        contentType: 'application/json',
        success: function (data) {
          if (global) {
            self.renderApplierState(data, true);
          } else {
            self.renderApplierState(data);
          }
        },
        error: function () {
          if (global) {
            arangoHelper.arangoError('Replication', 'Could not fetch the followers global applier state.');
          } else {
            arangoHelper.arangoError('Replication', 'Could not fetch the followers applier state.');
          }
        }
      });
    },

    renderApplierState: function (data, global) {
      var self = this;
      var endpoint;

      if (global) {
        data.database = 'All databases';
        data = {'All databases': data};
      }
      var errors = 0;
      var serverId;

      var a = [];
      var b = [];

      _.each(data, function (applier, db) {
        if (applier.state.running) {
          a.push(data[db]);
        } else {
          data[db].database = db;
          b.push(data[db]);
        }
      });

      a = _.sortBy(a, 'database');
      b = _.sortBy(b, 'database');

      data = a.concat(b);

      $('#repl-follower-table tbody').html('');
      _.each(data, function (applier, db) {
        if (applier.endpoint !== 'undefined' && applier.endpoint) {
          endpoint = self.parseEndpoint(applier.endpoint);
        } else {
          endpoint = 'not available';
        }

        if (applier.state.phase !== 'inactive') {
          if (applier.state.running !== true) {
            errors++;
          }
        }
        var health;
        var state = 'active';
        if (applier.state.phase === 'inactive') {
          state = 'inactive';
        }
        if (applier.state.lastError.errorNum === 0) {
          if (applier.state.phase === 'inactive' && !applier.state.running) {
            health = 'n/a';
          } else {
            health = '<i class="fa fa-check-circle positive"></i>';
          }
        } else {
          health = '<i class="fa fa-times-circle negative"></i>';
          errors++;
        }

        $('#repl-follower-table tbody').append(
          '<tr class="' + state + '">' +
            '<td id="applier-database-id">' + applier.database + '</td>' +
            '<td id="applier-running-id">' + applier.state.running + '</td>' +
            '<td>' + applier.state.phase + '</td>' +
            '<td id="applier-endpoint-id">' + endpoint + '</td>' +
            '<td>' + applier.state.lastAppliedContinuousTick + '</td>' +
            '<td>' + health + '</td>' +
          '</tr>'
        );
        serverId = applier.server.serverId;
      });

      $('#logger-lastLogTick-id').html(serverId);

      // health part
      if (errors === 0) {
        // all is fine
        this.renderHealth(false);
      } else {
        // some appliers are not running
        this.renderHealth(true, 'Some appliers are not running or do have errors.');
      }
    },

    renderHealth: function (errors, message) {
      if (errors) {
        $('#info-msg-id').addClass('negative');
        $('#info-msg-id').removeClass('positive');
        $('#info-msg-id').html('Bad <i class="fa fa-times-circle"></i>');

        if (message) {
          $('#info-msg-id').attr('title', message);
          $('#info-msg-id').addClass('modalTooltips');
          arangoHelper.createTooltips('.modalTooltips');
        }
      } else {
        $('#info-msg-id').addClass('positive');
        $('#info-msg-id').removeClass('negative');
        $('#info-msg-id').removeClass('modalTooltips');
        $('#info-msg-id').html('Good <i class="fa fa-check-circle"></i>');
      }
    },

    getStateData: function (cb) {
      // fetching mode 3 related information
      if (this.mode === 3) {
        this.getActiveFailoverEndpoints();
        this.getLoggerState();
      } else if (this.mode === 2) {
        if (this.info.role === 'leader') {
          this.getLoggerState();
        } else {
          // global follower
          this.getApplierStates(true);
        }
      } else if (this.mode === 1) {
        if (this.info.role === 'leader') {
          this.getLoggerState();
        } else {
          // single follower
          this.getApplierStates();
        }
      }
      if (cb) {
        cb();
      }
    },

    updateLoggerGraphsData: function (data) {
      if (this.loggerGraphsData.length > this.keepEntries) {
        this.loggerGraphsData.pop();
      }
      this.loggerGraphsData.push(data);
    },

    parseLoggerData: function () {
      var self = this;
      var datasets = this.loggerGraphsData;

      if (!this.colors) {
        this.colors = randomColor({
          hue: 'blue',
          count: this.loggerGraphsData.length
        });
      }

      var leader = 'Leader';

      var graphDataTime = {
        leader: {
          key: leader,
          values: [],
          strokeWidth: 2,
          color: '#2ecc71'
        }
      };

      var graphDataTick = {
        leader: {
          key: leader,
          values: [],
          strokeWidth: 2,
          color: '#2ecc71'
        }
      };

      _.each(datasets, function (data) {
        graphDataTime.leader.values.push({x: Date.parse(data.state.time), y: 0});
        graphDataTick.leader.values.push({x: Date.parse(data.state.time), y: 0});

        var colorCount = 0;
        _.each(data.clients, function (client) {
          if (!graphDataTime[client.serverId]) {
            var key = 'Follower (' + client.serverId + ')';
            graphDataTime[client.serverId] = {
              key: key,
              color: self.colors[colorCount],
              strokeWidth: 1,
              values: []
            };
            graphDataTick[client.serverId] = {
              key: key,
              color: self.colors[colorCount],
              strokeWidth: 1,
              values: []
            };
            colorCount++;
          }
          // time
          graphDataTime[client.serverId].values.push({
            x: Date.parse(client.time),
            y: (Date.parse(data.state.time) - Date.parse(client.time)) / 1000 * (-1)}
          );
          // ticks
          graphDataTick[client.serverId].values.push({
            x: Date.parse(client.time),
            y: (data.state.lastLogTick - client.lastServedTick) * (-1)}
          );
        });
      });

      return {
        graphDataTick: _.toArray(graphDataTick),
        graphDataTime: _.toArray(graphDataTime)
      };
    },

    initLoggerGraphs: function () {
      var self = this;

      // time chart
      nv.addGraph(function () {
        self.charts.replicationTimeChart = nv.models.lineChart()
          .options({
            duration: 300,
            useInteractiveGuideline: true,
            forceY: [2, -10]
          })
        ;
        self.charts.replicationTimeChart.xAxis
          .axisLabel('')
          .tickFormat(function (d) {
            var x = new Date(d);
            return (x.getHours() < 10 ? '0' : '') + x.getHours() + ':' +
              (x.getMinutes() < 10 ? '0' : '') + x.getMinutes() + ':' +
              (x.getSeconds() < 10 ? '0' : '') + x.getSeconds();
          })
          .staggerLabels(false);

        self.charts.replicationTimeChart.yAxis
          .axisLabel('Last call ago (in s)')
          .tickFormat(function (d) {
            if (d === null) {
              return 'N/A';
            }
            return d3.format(',.0f')(d);
          })
        ;
        var data = self.parseLoggerData().graphDataTime;
        d3.select('#replicationTimeChart svg')
          .datum(data)
          .call(self.charts.replicationTimeChart);
        nv.utils.windowResize(self.charts.replicationTimeChart.update);
        return self.charts.replicationTimeChart;
      });

      // tick chart
      nv.addGraph(function () {
        self.charts.replicationTickChart = nv.models.lineChart()
          .options({
            duration: 300,
            useInteractiveGuideline: true,
            forceY: [2, undefined]
          })
        ;
        self.charts.replicationTickChart.xAxis
          .axisLabel('')
          .tickFormat(function (d) {
            var x = new Date(d);
            return (x.getHours() < 10 ? '0' : '') + x.getHours() + ':' +
              (x.getMinutes() < 10 ? '0' : '') + x.getMinutes() + ':' +
              (x.getSeconds() < 10 ? '0' : '') + x.getSeconds();
          })
          .staggerLabels(false);

        self.charts.replicationTickChart.yAxis
          .axisLabel('Ticks behind')
          .tickFormat(function (d) {
            if (d === null) {
              return 'N/A';
            }
            return d3.format(',.0f')(d);
          })
        ;
        var data = self.parseLoggerData().graphDataTick;
        d3.select('#replicationTickChart svg')
          .datum(data)
          .call(self.charts.replicationTickChart);
        nv.utils.windowResize(self.charts.replicationTickChart.update);
        return self.charts.replicationTickChart;
      });

      self.nvchartsInit = true;
    },

    rerenderLoggerGraphs: function () {
      var self = this;
      // time chart
      d3.select('#replicationTimeChart svg')
        .datum(self.parseLoggerData().graphDataTime)
        .transition().duration(500)
        .call(this.charts.replicationTimeChart);

      // tick chart
      d3.select('#replicationTickChart svg')
        .datum(self.parseLoggerData().graphDataTick)
        .transition().duration(500)
        .call(this.charts.replicationTickChart);

      _.each(this.charts, function (chart) {
        nv.utils.windowResize(chart.update);
      });
    },

    renderLoggerState: function (server, clients, state) {
      if (server && clients && state) {
        // render logger information
        $('#logger-running-id').html(state.running);
        $('#logger-version-id').html(server.version);
        $('#logger-serverid-id').html(server.serverId);
        $('#logger-time-id').html(state.time);
        $('#logger-lastLogTick-id').html(state.lastLogTick);
        $('#logger-totalEvents-id').html(state.totalEvents);
        // render client information
        $('#repl-logger-clients tbody').html('');
        _.each(clients, function (client) {
          $('#repl-logger-clients tbody').append(
            '<tr><td>' + client.syncerId + '</td>' +
            '<td>' + client.serverId + '</td>' +
            '<td>' + client.clientInfo + '</td>' +
            '<td>' + client.time + '</td>' +
            '<td>' + client.lastServedTick + '</td></tr>'
          );
        });

        if (state.running) {
          this.renderHealth(false);
        } else {
          this.renderHealth(true, 'The logger thread is not running');
        }
      } else {
        $('#logger-running-id').html('Error');
        $('#logger-endpoint-id').html('Error');
        $('#logger-version-id').html('Error');
        $('#logger-serverid-id').html('Error');
        $('#logger-time-id').html('Error');
        this.renderHealth(true, 'Unexpected data from the logger thread.');
      }
    },

    getMode: function (callback) {
      var self = this;
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/aardvark/replication/mode'),
        contentType: 'application/json',
        success: function (data) {
          if (data.mode || data.mode === 0) {
            if (Number.isInteger(data.mode)) {
              self.mode = data.mode;
              if (data.mode !== 0) {
                self.info.state = 'enabled';
              } else {
                self.info.state = 'disabled';
              }
            } else {
              self.mode = 'undefined';
            }
            if (data.role) {
              self.info.role = data.role;
            }
            if (self.mode === 3) {
              self.info.mode = 'Active Failover';
              self.info.level = 'Server';
            } else if (self.mode === 2) {
              self.info.mode = 'Leader/Follower';
              self.info.level = 'Server';
            } else if (self.mode === 1) {
              self.info.mode = 'Leader/Follower';
              if (self.info.role === 'follower') {
                self.info.level = 'Database';
              } else {
                // self.info.level = 'Database/Server';
                self.info.level = 'Check followers for details';
              }
            }
          }
          if (callback) {
            callback();
          }
        },
        error: function () {
          arangoHelper.arangoError('Replication', 'Could not fetch the replication state.');
        }
      });
    }

  });
}());
