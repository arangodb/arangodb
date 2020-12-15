/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, templateEngine, window, arangoHelper, $, _ */
(function () {
  'use strict';

  window.FilterSelectView = Backbone.View.extend({
    el: '#filterSelectDiv',
    filterOptionsEl: '.filterOptions',

    initialize: function (options) {
      this.name = options.name;
      this.options = options.options;
      this.position = options.position;
      this.callback = options.callback;
      this.multiple = options.multiple;
    },

    /* options arr elem
     *  option: {
     *    name: <string>,
     *    active: <boolean,
     *    color: <string>
     *  }
     */

    template: templateEngine.createTemplate('filterSelect.ejs'),

    events: {
      'click .filterOptions .inactive': 'changeState',
      'click .filterOptions .active': 'changeState',
      'click #showAll': 'showAll',
      'click #closeFilter': 'hide',
      'keyup .filterInput input': 'filter'
    },

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    changeState: function (e) {
      var self = this;
      var name = $(e.currentTarget).attr('name');
      if ($(e.currentTarget).hasClass('active')) {
        self.options[name].active = false;
        $(e.currentTarget).removeClass('active').addClass('inactive');
        $(e.currentTarget).find('.marked').css('visibility', 'hidden');
      } else {
        self.options[name].active = true;
        $(e.currentTarget).removeClass('inactive').addClass('active');
        $(e.currentTarget).find('.marked').css('visibility', 'visible');
      }

      if (this.callback) {
        this.callback(this.options);
      }
    },

    filter: function () {
      var value = $('#' + this.name + '-filter').val();
      _.each(this.options, function (option) {
        if (option.name.search(value) > -1) {
          $('#' + option.name + '-option').css('display', 'block');
        } else {
          $('#' + option.name + '-option').css('display', 'none');
        }
      });
    },

    clearFilter: function () {
      $('#' + this.name + '-filter').val('');
      this.filter();
    },

    showAll: function () {
      this.clearFilter();
      _.each(this.options, function (option) {
        option.active = false;
        $('#' + option.name + '-option').removeClass('active').addClass('inactive');
        $('#' + option.name + '-option').find('.marked').css('visibility', 'hidden');
      });
      this.callback(this.options);
    },

    render: function () {
      var self = this;

      $('#filterSelectDiv').on('click', function (e) {
        if (e.target.id === 'filterSelectDiv') {
          self.hide();
        }
      });

      _.each(self.options, function (option) {
        if (!option.color) {
          option.color = arangoHelper.alphabetColors[option.name.charAt(0).toLowerCase()];
        }
      });

      this.$el.html(this.template.render({
        name: self.name,
        options: self.options
      }));

      $('#filterSelectDiv > div').css('right', this.position.right + 'px');
      $('#filterSelectDiv > div').css('top', this.position.top + 30);

      this.show();
      $('#' + this.name + '-filter').focus();
    },

    show: function () {
      $(this.el).show();
    },

    hide: function () {
      $('#filterSelectDiv').unbind('click');
      $(this.el).hide();
      this.remove();
    }

  });
}());
