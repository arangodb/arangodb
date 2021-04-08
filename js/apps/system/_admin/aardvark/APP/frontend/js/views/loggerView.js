/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, arangoHelper, $, _, window, templateEngine */

(function () {
  'use strict';

  window.LoggerView = Backbone.View.extend({
    el: '#content',
    logsel: '#logEntries',
    initDone: false,

    fetchedEntries: [],
    pageSize: 20,
    currentPage: 0,

    logTopics: {},
    logLevels: [],

    initialize: function (options) {
      var self = this;

      if (options) {
        this.options = options;

        if (options.endpoint) {
          this.endpoint = options.endpoint;
        }

        if (options.contentDiv) {
          this.el = options.contentDiv;
        }
      }

      this.collection.setPageSize(this.pageSize);

      if (!this.initDone) {
        let url = arangoHelper.databaseUrl('/_admin/log/level');
        if (this.endpoint) {
          url += `?serverId=${encodeURIComponent(this.endpoint)}`;
        }
        // first fetch all log topics + topics
        $.ajax({
          type: 'GET',
          cache: false,
          url: url,
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
      'click #loadMoreEntries': 'loadMoreEntries',
      'click #downloadDisplayedEntries': 'downloadEntries'
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

    downloadEntries: function () {
      // sort entries (primary by date, then by lid)
      this.fetchedEntries.sort(function compare(a, b) {
        let dateA = new Date(a.date);
        let dateB = new Date(b.date);
        return dateB - dateA || b.lid - a.lid;
      });

      let currentDate = new Date();
      let fileName = `LOGS-${currentDate.toISOString()}`;
      arangoHelper.downloadLocalBlob(JSON.stringify(this.fetchedEntries, null, 2), 'json', fileName);
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
      var entriesToAppend = [];

      this.collection.fetch({
        success: function (settings) {
          self.collection.each(function (model) {
            date = new Date(model.get('timestamp') * 1000);
            let entry = {
              status: model.getLogStatus(),
              date: arangoHelper.formatDT(date),
              timestamp: model.get('timestamp'),
              msg: model.get('text'),
              topic: model.get('topic')
            };
            entriesToAppend.push(entry);

            // keep history for export
            self.fetchedEntries.push(model.toJSON());
          });
          // invert order
          self.renderLogs(self.invertArray(entriesToAppend), settings.lastInverseOffset);
        }
      });
    },

    render: function (initialRender) {
      var self = this;
      if (initialRender) {
        this.currentPage = 0;
        this.fetchedEntries = [];
      }

      if (this.initDone) {
        // render static content
        $(this.el).html(this.template.render({}));

        // fetch dyn. content
        this.convertModelToJSON();
      } else {
        window.setTimeout(function () {
          self.render(false);
        }, 100);
      }
      return this;
    },

    renderLogs: function (entries, offset) {
      _.each(entries, function (entry) {
        if (entry.msg.indexOf('{' + entry.topic + '}') > -1) {
          entry.msg = entry.msg.replace('{' + entry.topic + '}', '');
        }
        entry.msg = arangoHelper.escapeHtml(entry.msg);
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
