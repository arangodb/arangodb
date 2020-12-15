/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window, _ */
(function () {
  'use strict';

  window.NodesView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('nodesView.ejs'),
    interval: 10000,
    knownServers: [],

    events: {
      'click #nodesContent .coords-nodes .pure-table-row': 'navigateToNode',
      'click #nodesContent .dbs-nodes .pure-table-row': 'navigateToNode',
      'click #nodesContent .coords-nodes .pure-table-row .fa-trash-o': 'deleteNode',
      'click #nodesContent .dbs-nodes .pure-table-row .fa-trash-o': 'deleteNode',
      'click #addCoord': 'addCoord',
      'click #removeCoord': 'removeCoord',
      'click #addDBs': 'addDBs',
      'click #removeDBs': 'removeDBs',
      'click .abortClusterPlan': 'abortClusterPlanModal',
      'keyup #plannedCoords': 'checkKey',
      'keyup #plannedDBs': 'checkKey'
    },

    remove: function () {
      clearInterval(this.intervalFunction);
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    checkKey: function (e) {
      if (e.keyCode === 13) {
        var self = this;

        var callbackFunction = function (e) {
          var number;
          if (e.target.id === 'plannedCoords') {
            try {
              number = JSON.parse($('#plannedCoords').val());
              if (typeof number === 'number') {
                window.modalView.hide();
                self.setCoordSize(number);
              } else {
                arangoHelper.arangoError('Error', 'Invalid value. Must be a number.');
              }
            } catch (e) {
              arangoHelper.arangoError('Error', 'Invalid value. Must be a number.');
            }
          } else if (e.target.id === 'plannedDBs') {
            try {
              number = JSON.parse($('#plannedCoords').val());
              if (typeof number === 'number') {
                window.modalView.hide();
                self.setDBsSize(number);
              } else {
                arangoHelper.arangoError('Error', 'Invalid value. Must be a number.');
              }
            } catch (e) {
              arangoHelper.arangoError('Error', 'Invalid value. Must be a number.');
            }
          }
        };

        this.changePlanModal(callbackFunction.bind(null, e));
      }
    },

    changePlanModal: function (func, element) {
      var buttons = []; var tableContent = [];
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          'plan-confirm-button',
          'Caution',
          'You are changing the cluster plan. Continue?',
          undefined,
          undefined,
          false,
          /[<>&'"]/
        )
      );
      buttons.push(
        window.modalView.createSuccessButton('Yes', func.bind(this, element))
      );
      window.modalView.show('modalTable.ejs', 'Modify Cluster Size', buttons, tableContent);
    },

    initialize: function () {
      var self = this;
      clearInterval(this.intervalFunction);

      if (window.App.isCluster) {
        this.updateServerTime();

        // start polling with interval
        this.intervalFunction = window.setInterval(function () {
          if (window.location.hash === '#nodes') {
            self.render(false);
          }
        }, this.interval);
      }
    },

    deleteNode: function (elem) {
      if ($(elem.currentTarget).hasClass('noHover')) {
        return;
      }
      var self = this;
      var name = $(elem.currentTarget.parentNode.parentNode).attr('node').slice(0, -5);
      if (window.confirm('Do you want to delete this node?')) {
        $.ajax({
          type: 'POST',
          url: arangoHelper.databaseUrl('/_admin/cluster/removeServer'),
          contentType: 'application/json',
          async: true,
          data: JSON.stringify(name),
          success: function (data) {
            self.render(false);
          },
          error: function () {
            if (window.location.hash === '#nodes') {
              arangoHelper.arangoError('Cluster', 'Could not fetch cluster information');
            }
          }
        });
      }
      return false;
    },

    navigateToNode: function (elem) {
      var name = $(elem.currentTarget).attr('node').slice(0, -5);

      if ($(elem.currentTarget).hasClass('noHover')) {
        return;
      }

      window.App.navigate('#node/' + encodeURIComponent(name), {trigger: true});
    },

    render: function (navi) {
      if (window.location.hash === '#nodes') {
        var self = this;

        if ($('#content').is(':empty')) {
          arangoHelper.renderEmpty('Please wait. Requesting cluster information...', 'fa fa-spin fa-circle-o-notch');
        }

        var scalingFunc = function (nodes) {
          $.ajax({
            type: 'GET',
            url: arangoHelper.databaseUrl('/_admin/cluster/numberOfServers'),
            contentType: 'application/json',
            success: function (data) {
              if (window.location.hash === '#nodes') {
                if (navi !== false) {
                  arangoHelper.buildNodesSubNav('Overview', false);
                }
                self.continueRender(nodes, data);
              }
            },
            error: function (err) {
              if (navi !== false) {
                arangoHelper.buildNodesSubNav('Overview', true);
              }
              self.renderFailure(err);
            }
          });
        };

        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_admin/cluster/health'),
          contentType: 'application/json',
          processData: false,
          async: true,
          success: function (data) {
            if (window.location.hash === '#nodes') {
              scalingFunc(data.Health);
            }
          },
          error: function () {
            if (window.location.hash === '#nodes') {
              arangoHelper.arangoError('Cluster', 'Could not fetch cluster information');
            }
          }
        });
      }
    },

    renderFailure: function (err) {
      var title = 'Unexpected Failure';
      var msg = 'See the browser developer console for more details.';

      $(this.el).html('');
      if (err.responseJSON && err.responseJSON.code === 403) {
        title = 'Forbidden';
        msg = 'No access to read nodes data.';
      } else if (err.responseJSON && err.responseJSON.code && err.responseJSON.errorMessage) {
        title = 'Error code: ' + err.responseJSON.code;
        msg = err.responseJSON.errorMessage;
      } else {
        console.log(err);
      }

      // render info text
      $(this.el).append(
        '<div class="infoBox errorBox" style="background: white;">' +
        '<h4>' + title + '</h4>' +
        '<p>' + msg + '</p>' +
        '</div>'
      );
    },

    continueRender: function (nodes, scaling) {
      var coords = [];
      var dbs = [];
      var scale = false;

      _.each(nodes, function (node, name) {
        node.id = name;
        if (node.Role === 'Coordinator') {
          coords.push(node);
        } else if (node.Role === 'DBServer') {
          dbs.push(node);
        }
      });
      coords = _.sortBy(coords, function (o) { return o.ShortName; });
      dbs = _.sortBy(dbs, function (o) { return o.ShortName; });

      if (scaling.numberOfDBServers !== null && scaling.numberOfCoordinators !== null) {
        scale = true;
      }

      var callback = function (scaleProperties) {
        this.$el.html(this.template.render({
          coords: coords,
          dbs: dbs,
          scaling: scale,
          scaleProperties: scaleProperties,
          plannedDBs: scaling.numberOfDBServers,
          plannedCoords: scaling.numberOfCoordinators
        }));

        if (!scale) {
          $('.title').css('position', 'relative');
          $('.title').css('top', '-4px');
          $('.sectionHeader .information').css('margin-top', '-3px');
        }
      }.bind(this);

      this.renderCounts(scale, callback);
    },

    updatePlanned: function (data) {
      if (data.numberOfCoordinators) {
        $('#plannedCoords').val(data.numberOfCoordinators);
        this.renderCounts(true);
      }
      if (data.numberOfDBServers) {
        $('#plannedDBs').val(data.numberOfDBServers);
        this.renderCounts(true);
      }
    },

    setCoordSize: function (number) {
      var self = this;
      var data = {
        numberOfCoordinators: number
      };

      $.ajax({
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_admin/cluster/numberOfServers'),
        contentType: 'application/json',
        data: JSON.stringify(data),
        success: function () {
          self.updatePlanned(data);
        },
        error: function () {
          arangoHelper.arangoError('Scale', 'Could not set coordinator size.');
        }
      });
    },

    setDBsSize: function (number) {
      var self = this;
      var data = {
        numberOfDBServers: number
      };

      $.ajax({
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_admin/cluster/numberOfServers'),
        contentType: 'application/json',
        data: JSON.stringify(data),
        success: function () {
          self.updatePlanned(data);
        },
        error: function () {
          arangoHelper.arangoError('Scale', 'Could not set coordinator size.');
        }
      });
    },

    abortClusterPlanModal: function () {
      var buttons = []; var tableContent = [];
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          'plan-abort-button',
          'Caution',
          'You are aborting the planned cluster plan. All pending servers are going to be removed. Continue?',
          undefined,
          undefined,
          false,
          /[<>&'"]/
        )
      );
      buttons.push(
        window.modalView.createSuccessButton('Yes', this.abortClusterPlan.bind(this))
      );
      window.modalView.show('modalTable.ejs', 'Modify Cluster Size', buttons, tableContent);
    },

    abortClusterPlan: function () {
      window.modalView.hide();
      try {
        var coords = JSON.parse($('#infoCoords > .positive > span').text());
        var dbs = JSON.parse($('#infoDBs > .positive > span').text());
        this.setCoordSize(coords);
        this.setDBsSize(dbs);
      } catch (ignore) {
        arangoHelper.arangoError('Plan', 'Could not abort Cluster Plan');
      }
    },

    renderCounts: function (scale, callback) {
      var self = this;
      var renderFunc = function (id, ok, pending, error) {
        var string = '<span class="positive"><span>' + ok + '</span><i class="fa fa-check-circle"></i></span>';
        if (pending && scale === true) {
          string = string + '<span class="warning"><span>' + pending +
            '</span><i class="fa fa-circle-o-notch fa-spin"></i></span><button class="abortClusterPlan button-navbar button-default">Abort</button>';
        }
        if (error) {
          string = string + '<span class="negative"><span>' + error + '</span><i class="fa fa-exclamation-circle"></i></span>';
        }
        $(id).html(string);

        if (!scale) {
          $('.title').css('position', 'relative');
          $('.title').css('top', '-4px');
        }
      };

      var callbackFunction = function (nodes) {
        var coordsErrors = 0;
        var coords = 0;
        var coordsPending = 0;
        var dbs = 0;
        var dbsErrors = 0;
        var dbsPending = 0;

        _.each(nodes, function (node) {
          if (node.Role === 'Coordinator') {
            if (node.Status === 'GOOD') {
              coords++;
            } else {
              coordsErrors++;
            }
          } else if (node.Role === 'DBServer') {
            if (node.Status === 'GOOD') {
              dbs++;
            } else {
              dbsErrors++;
            }
          }
        });

        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_admin/cluster/numberOfServers'),
          contentType: 'application/json',
          processData: false,
          success: function (data) {
            coordsPending = Math.abs((coords + coordsErrors) - data.numberOfCoordinators);
            dbsPending = Math.abs((dbs + dbsErrors) - data.numberOfDBServers);

            if (callback) {
              callback({
                coordsPending: coordsPending,
                coordsOk: coords,
                coordsErrors: coordsErrors,
                dbsPending: dbsPending,
                dbsOk: dbs,
                dbsErrors: dbsErrors
              });
            } else {
              renderFunc('#infoDBs', dbs, dbsPending, dbsErrors);
              renderFunc('#infoCoords', coords, coordsPending, coordsErrors);
            }

            if (!self.isPlanFinished()) {
              $('.scaleGroup').addClass('no-hover');
              $('#plannedCoords').attr('disabled', 'disabled');
              $('#plannedDBs').attr('disabled', 'disabled');
            }
          }
        });
      };

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/health'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callbackFunction(data.Health);
        }
      });
    },

    isPlanFinished: function () {
      var boolean;

      if ($('#infoDBs').find('.warning').length > 0) {
        boolean = false;
      } else if ($('#infoCoords').find('.warning').length > 0) {
        boolean = false;
      } else {
        boolean = true;
      }

      return boolean;
    },

    addCoord: function () {
      var func = function () {
        window.modalView.hide();
        this.setCoordSize(this.readNumberFromID('#plannedCoords', true));
      };

      if (this.isPlanFinished()) {
        this.changePlanModal(func.bind(this));
      } else {
        arangoHelper.arangoNotification('Cluster Plan', 'Planned state not yet finished.');
        $('.noty_buttons .button-danger').remove();
      }
    },

    removeCoord: function () {
      var func = function () {
        window.modalView.hide();
        this.setCoordSize(this.readNumberFromID('#plannedCoords', false, true));
      };

      if (this.isPlanFinished()) {
        this.changePlanModal(func.bind(this));
      } else {
        arangoHelper.arangoNotification('Cluster Plan', 'Planned state not yet finished.');
        $('.noty_buttons .button-danger').remove();
      }
    },

    addDBs: function () {
      var func = function () {
        window.modalView.hide();
        this.setDBsSize(this.readNumberFromID('#plannedDBs', true));
      };

      if (this.isPlanFinished()) {
        this.changePlanModal(func.bind(this));
      } else {
        arangoHelper.arangoNotification('Cluster Plan', 'Planned state not yet finished.');
        $('.noty_buttons .button-danger').remove();
      }
    },

    removeDBs: function () {
      var func = function () {
        window.modalView.hide();
        this.setDBsSize(this.readNumberFromID('#plannedDBs', false, true));
      };

      if (this.isPlanFinished()) {
        this.changePlanModal(func.bind(this));
      } else {
        arangoHelper.arangoNotification('Cluster Plan', 'Planned state not yet finished.');
        $('.noty_buttons .button-danger').remove();
      }
    },

    readNumberFromID: function (id, increment, decrement) {
      var value = $(id).val();
      var parsed = false;

      try {
        parsed = JSON.parse(value);
      } catch (ignore) {}

      if (increment) {
        parsed++;
      }
      if (decrement) {
        if (parsed !== 1) {
          parsed--;
        }
      }

      return parsed;
    },

    updateServerTime: function () {
      this.serverTime = new Date().getTime();
    }

  });
}());
