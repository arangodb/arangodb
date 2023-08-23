/* global templateEngine, frontendConfig */
(function () {
  'use strict';

  window.MaintenanceView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate('maintenanceView.ejs'),
    interval: 10000,

    events: {
      'click #enableMaintenanceMode': 'enableMaintenanceMode',
      'click #disableMaintenanceMode': 'disableMaintenanceMode'
    },

    initialize: function (options) {
      var self = this;
      clearInterval(this.intervalFunction);

      if (window.App.isCluster) {
        // start polling with interval
        this.intervalFunction = window.setInterval(function () {
          if (window.location.hash === '#maintenance') {
            self.render(false);
          }
        }, this.interval);
      }
    },

    remove: function () {
      clearInterval(this.intervalFunction);
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    render: function (navi) {
      if (window.location.hash === '#maintenance') {
        var self = this;

        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_admin/cluster/maintenance'),
          contentType: 'application/json',
          processData: false,
          async: true,
          success: function (data) {
            self.continueRender(data.result, false);
          },
          error: function (data) {
            self.continueRender(data, true);
          }
        });

        if (navi !== false) {
          arangoHelper.buildClusterSubNav('Maintenance');
        }
      }
    },

    enableMaintenanceMode: function () {
      this.changeMaintenance("on");
    },

    disableMaintenanceMode: function () {
      this.changeMaintenance("off");
    },

    changeMaintenance: function (mode) {
      let self = this;
      let buttons = [];
      let tableContent = [];

      let title;

      if (mode === 'off') {
        title = 'Disable Maintenance Mode';
      } else {
        title = 'Enable Maintenance Mode';
      }

      let info = `<blockquote>
        Modifies the cluster supervision maintenance mode. Be aware that no automatic failovers of any
        kind will take place while the maintenance mode is enabled. The cluster supervision reactivates
        itself automatically after the selected duration in case it is not manually prolonged.<blockquote>
      `;

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          'change-maintenance-button',
          title,
          info
        )
      );
         
      if (mode !== 'off') {
        // when enabling maintenance mode, add a selectbox with some useful
        // maintenance durations for the user to pick from. we don't need this
        // when turning the maintenance off.
        tableContent.push(
          window.modalView.createSelectEntry(
            'maintenance-duration',
            'Maintenance duration',
            3600,
            'The duration that the supervision maintenance will be enabled.',
            [{value: 600, label: '10 minutes'}, {value: 1200, label: '20 minutes'},
             {value: 1800, label: '30 minutes'}, {value: 2700, label: '45 minutes'},
             {value: 3600, label: '1 hour'}, {value: 5400, label: '1.5 hours'},
             {value: 7200, label: '2 hours'}, {value: 10800, label: '3 hours'},
             {value: 14400, label: '4 hours'}, {value: 18000, label: '5 hours'},
             {value: 36000, label: '10 hours'}, {value: 86400, label: '24 hours'},
            ]
          )
        );
        
        buttons.push(
          window.modalView.createNotificationButton(
            title,
            self.confirmChangeMaintenance.bind(this, mode)
          )
        );
      } else {
        // disable maintenance mode
        buttons.push(
          window.modalView.createSuccessButton(
            title,
            self.confirmChangeMaintenance.bind(this, mode)
          )
        );
      }

      window.modalView.show(
        'modalTable.ejs',
        'Supervision Maintenance Mode',
        buttons,
        tableContent
      );
    },

    confirmChangeMaintenance: function (mode) {
      let self = this;
      let data = mode;

      if (mode !== 'off') {
        data = $('#maintenance-duration').val();
      }

      $.ajax({
        type: 'PUT',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/maintenance'),
        contentType: 'application/json',
        processData: false,
        data: JSON.stringify(data),
        async: true,
        success: function () {
          arangoHelper.arangoNotification('Maintenance', 'Set supervision maintenance mode to: ' + mode);
          self.render();
        },
        error: function () {
          arangoHelper.arangoError('Maintenance', 'Could not change supervision maintenance mode!');
        }
      });

      window.modalView.hide();
    },

    continueRender: function (maintenanceMode, error) {
      this.$el.html(this.template.render({
        maintenanceMode: (maintenanceMode !== 'Normal'),
        error: error,
        canChange: frontendConfig.clusterApiJwtPolicy === 'jwt-compat'
      }));
    }
  });
}());
