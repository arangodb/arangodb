/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, arangoHelper, $, _, window, templateEngine */

(function () {
  'use strict';

  window.LoggerView = Backbone.View.extend({
    el: '#content',
    logsel: '#logEntries',
    id: '#logContent',
    initDone: false,

    pageSize: 20,
    currentPage: 0,

    logTopics: {},
    logLevels: [],

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    initialize: function (options) {
      var self = this;

      if (options) {
        this.options = options;
      }

      this.collection.setPageSize(this.pageSize);

      if (!this.initDone) {
        // first fetch all log topics + topics
        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_admin/log/level'),
          contentType: 'application/json',
          processData: false,
          success: function (data) {
            self.logTopics = data;

            _.each(['fatal', 'error', 'warning', 'info', 'debug'], function (level) {
              self.logLevels.push(level);
            });

            self.initDone = true;
          }
        });
      }
    },

    currentLoglevel: undefined,
    defaultLoglevel: 'info',

    events: {
      'click #logLevelSelection': 'renderLogLevel',
      'click #logTopicSelection': 'renderLogTopic',
      'click #logFilters': 'resetFilters',
      'click #loadMoreEntries': 'loadMoreEntries'
    },

    template: templateEngine.createTemplate('loggerView.ejs'),
    templateEntries: templateEngine.createTemplate('loggerViewEntries.ejs'),

    renderLogTopic: function (e) {
      var self = this;

      if (!this.logTopicOptions) {
        this.logTopicOptions = {};
      }

      var active;
      _.each(this.logTopics, function (topic, name) {
        if (self.logTopicOptions[name]) {
          active = self.logTopicOptions[name].active;
        }
        self.logTopicOptions[name] = {
          name: name,
          active: active || false
        };
      });

      var pos = $(e.currentTarget).position();
      pos.right = '30';

      this.logTopicView = new window.FilterSelectView({
        name: 'Topic',
        options: self.logTopicOptions,
        position: pos,
        callback: self.logTopicCallbackFunction.bind(this),
        multiple: true
      });
      this.logTopicView.render();
    },

    loadMoreEntries: function () {
      this.convertModelToJSON();
    },

    logTopicCallbackFunction: function (options) {
      this.logTopicOptions = options;
      this.applyFilter();
    },

    logLevelCallbackFunction: function (options) {
      this.logLevelOptions = options;
      this.applyFilter();
    },

    resetFilters: function () {
      _.each(this.logTopicOptions, function (option) {
        option.active = false;
      });
      _.each(this.logLevelOptions, function (option) {
        option.active = false;
      });
      this.applyFilter();
    },

    isFilterActive: function (filterobj) {
      var active = false;
      _.each(filterobj, function (obj) {
        if (obj.active) {
          active = true;
        }
      });
      return active;
    },

    changeButton: function (btn) {
      if (!btn) {
        $('#logTopicSelection').addClass('button-default').removeClass('button-success');
        $('#logLevelSelection').addClass('button-default').removeClass('button-success');
        $('#logFilters').hide();
        $('#filterDesc').html('');
      } else {
        if (btn === 'level') {
          $('#logLevelSelection').addClass('button-success').removeClass('button-default');
          $('#logTopicSelection').addClass('button-default').removeClass('button-success');
          $('#filterDesc').html(btn);
        } else if (btn === 'topic') {
          $('#logTopicSelection').addClass('button-success').removeClass('button-default');
          $('#logLevelSelection').addClass('button-default').removeClass('button-success');
          $('#filterDesc').html(btn);
        } else if (btn === 'both') {
          $('#logTopicSelection').addClass('button-success').removeClass('button-default');
          $('#logLevelSelection').addClass('button-success').removeClass('button-default');
          $('#filterDesc').html('level, topic');
        }
        $('#logFilters').show();
      }
    },

    applyFilter: function () {
      var self = this;
      var isLevel = this.isFilterActive(this.logLevelOptions);
      var isTopic = this.isFilterActive(this.logTopicOptions);

      if (isLevel && isTopic) {
        // both filters active
        _.each($('#logEntries').children(), function (entry) {
          if (self.logLevelOptions[$(entry).attr('level')].active === false || self.logTopicOptions[$(entry).attr('topic')].active === false) {
            $(entry).hide();
          } else if (self.logLevelOptions[$(entry).attr('level')].active && self.logTopicOptions[$(entry).attr('topic')].active) {
            $(entry).show();
          }
        });
        this.changeButton('both');
      } else if (isLevel && !isTopic) {
        // only level filter active
        _.each($('#logEntries').children(), function (entry) {
          if (self.logLevelOptions[$(entry).attr('level')].active === false) {
            $(entry).hide();
          } else {
            $(entry).show();
          }
        });
        this.changeButton('level');
      } else if (!isLevel && isTopic) {
        // only topic filter active
        _.each($('#logEntries').children(), function (entry) {
          if (self.logTopicOptions[$(entry).attr('topic')].active === false) {
            $(entry).hide();
          } else {
            $(entry).show();
          }
        });
        this.changeButton('topic');
      } else if (!isLevel && !isTopic) {
        _.each($('#logEntries').children(), function (entry) {
          $(entry).show();
        });
        this.changeButton();
      }

      var count = 0;
      _.each($('#logEntries').children(), function (elem) {
        if ($(elem).css('display') === 'flex') {
          $(elem).css('display', 'block');
        }
        if ($(elem).css('display') === 'block') {
          count++;
        }
      });
      if (count === 1) {
        $('.logBorder').css('visibility', 'hidden');
        $('#noLogEntries').hide();
      } else if (count === 0) {
        $('#noLogEntries').show();
      } else {
        $('.logBorder').css('visibility', 'visible');
        $('#noLogEntries').hide();
      }
    },

    renderLogLevel: function (e) {
      var self = this;

      if (!this.logLevelOptions) {
        this.logLevelOptions = {};
      }

      var active;
      _.each(this.logLevels, function (name) {
        if (self.logLevelOptions[name]) {
          active = self.logLevelOptions[name].active;
        }
        self.logLevelOptions[name] = {
          name: name,
          active: active || false
        };

        var color = arangoHelper.statusColors[name];

        if (color) {
          self.logLevelOptions[name].color = color;
        }
      });

      var pos = $(e.currentTarget).position();
      pos.right = '115';

      this.logLevelView = new window.FilterSelectView({
        name: 'Level',
        options: self.logLevelOptions,
        position: pos,
        callback: self.logLevelCallbackFunction.bind(this),
        multiple: false
      });
      this.logLevelView.render();
    },

    setActiveLoglevel: function (e) {

    },

    initTotalAmount: function () {
      var self = this;
      this.collection.fetch({
        data: $.param(
          {test: true}
        ),
        success: function () {
          self.convertModelToJSON();
        }
      });
      this.fetchedAmount = true;
    },

    invertArray: function (array) {
      var rtnArr = [];
      var counter = 0;
      var i;

      for (i = array.length - 1; i >= 0; i--) {
        rtnArr[counter] = array[i];
        counter++;
      }
      return rtnArr;
    },

    convertModelToJSON: function () {
      if (!this.fetchedAmount) {
        this.initTotalAmount();
        return;
      }

      this.collection.page = this.currentPage;
      this.currentPage++;

      var self = this;
      var date;
      var entries = [];
      this.collection.fetch({
        success: function (settings) {
          self.collection.each(function (model) {
            date = new Date(model.get('timestamp') * 1000);
            entries.push({
              status: model.getLogStatus(),
              date: arangoHelper.formatDT(date),
              timestamp: model.get('timestamp'),
              msg: model.get('text'),
              topic: model.get('topic')
            });
          });
          // invert order
          self.renderLogs(self.invertArray(entries), settings.lastInverseOffset);
        }
      });
    },

    render: function () {
      var self = this;
      this.currentPage = 0;

      if (this.initDone) {
        // render static content
        $(this.el).html(this.template.render({}));

        // fetch dyn. content
        this.convertModelToJSON();
      } else {
        window.setTimeout(function () {
          self.render();
        }, 100);
      }
      return this;
    },

    renderLogs: function (entries, offset) {
      _.each(entries, function (entry) {
        if (entry.msg.indexOf('{' + entry.topic + '}') > -1) {
          entry.msg = entry.msg.replace('{' + entry.topic + '}', '');
        }
      });

      if (this.currentPage === 0) {
        // render initial
        $(this.logsel).html(this.templateEntries.render({
          entries: entries
        }));
      } else {
        // append
        $(this.logsel).append(this.templateEntries.render({
          entries: entries
        }));
      }

      if (offset <= 0) {
        $('#loadMoreEntries').attr('disabled', true);
      } else {
        $('#loadMoreEntries').attr('disabled', false);
      }
      arangoHelper.createTooltips();
      this.applyFilter();
    }

  });
}());
