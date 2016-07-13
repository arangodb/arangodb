/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, window */
/* global templateEngine */

(function () {
  'use strict';
  window.WorkMonitorView = Backbone.View.extend({
    el: '#content',
    id: '#workMonitorContent',

    template: templateEngine.createTemplate('workMonitorView.ejs'),
    table: templateEngine.createTemplate('arangoTable.ejs'),

    initialize: function () {},

    events: {
    },

    tableDescription: {
      id: 'workMonitorTable',
      titles: [
        'Type', 'Database', 'Task ID', 'Started', 'Url', 'User', 'Description', 'Method'
      ],
      rows: [],
      unescaped: [false, false, false, false, false, false, false, false]
    },

    render: function () {
      var self = this;

      this.$el.html(this.template.render({}));
      this.collection.fetch({
        success: function () {
          self.parseTableData();
          $(self.id).append(self.table.render({content: self.tableDescription}));
        }
      });
    },

    parseTableData: function () {
      var self = this;

      this.collection.each(function (model) {
        if (model.get('type') === 'AQL query') {
          var parent = model.get('parent');
          if (parent) {
            try {
              self.tableDescription.rows.push([
                model.get('type'),
                '(p) ' + parent.database,
                '(p) ' + parent.taskId,
                '(p) ' + parent.startTime,
                '(p) ' + parent.url,
                '(p) ' + parent.user,
                model.get('description'),
                '(p) ' + parent.method
              ]);
            } catch (e) {
              console.log('some parse error');
            }
          }
        } else if (model.get('type') !== 'thread') {
          self.tableDescription.rows.push([
            model.get('type'),
            model.get('database'),
            model.get('taskId'),
            model.get('startTime'),
            model.get('url'),
            model.get('user'),
            model.get('description'),
            model.get('method')
          ]);
        }
      });
    }

  });
}());
