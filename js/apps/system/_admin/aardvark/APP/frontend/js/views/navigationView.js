/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, templateEngine, $, window, arangoHelper, _ */
(function () {
  'use strict';
  window.NavigationView = Backbone.View.extend({
    el: '#navigationBar',
    subEl: '#subNavigationBar',

    events: {
      'change #arangoCollectionSelect': 'navigateBySelect',
      'click .tab': 'navigateByTab',
      'click li': 'switchTab',
      'click .arangodbLogo': 'selectMenuItem',
      'mouseenter .dropdown > *': 'showDropdown',
      'click .shortcut-icons p': 'showShortcutModal',
      'mouseleave .dropdown': 'hideDropdown'
    },

    renderFirst: true,
    activeSubMenu: undefined,

    changeDB: function () {
      window.location.hash = '#login';
    },

    initialize: function (options) {
      var self = this;

      this.userCollection = options.userCollection;
      this.currentDB = options.currentDB;
      this.dbSelectionView = new window.DBSelectionView({
        collection: options.database,
        current: this.currentDB
      });
      this.userBarView = new window.UserBarView({
        userCollection: this.userCollection
      });
      this.notificationView = new window.NotificationView({
        collection: options.notificationCollection
      });
      this.statisticBarView = new window.StatisticBarView({
        currentDB: this.currentDB
      });

      this.isCluster = options.isCluster;
      this.foxxApiEnabled = options.foxxApiEnabled;

      this.handleKeyboardHotkeys();

      Backbone.history.on('all', function () {
        self.selectMenuItem();
      });
    },

    showShortcutModal: function () {
      arangoHelper.hotkeysFunctions.showHotkeysModal();
    },

    handleSelectDatabase: function () {
      this.dbSelectionView.render($('#dbSelect'));
    },

    template: templateEngine.createTemplate('navigationView.ejs'),
    templateSub: templateEngine.createTemplate('subNavigationView.ejs'),

    render: function () {
      $(this.subEl).html(this.templateSub.render({
        currentDB: this.currentDB.toJSON()
      }));
      arangoHelper.checkDatabasePermissions(this.continueRender.bind(this), this.continueRender.bind(this));
    },

    continueRender: function (readOnly) {
      var self = this;

      $(this.el).html(this.template.render({
        currentDB: this.currentDB,
        isCluster: this.isCluster,
        foxxApiEnabled: this.foxxApiEnabled,
        readOnly: readOnly
      }));

      if (this.currentDB.get('name') !== '_system') {
        $('#dashboard').parent().remove();
      }

      this.dbSelectionView.render($('#dbSelect'));
      // this.notificationView.render($("#notificationBar"))

      var callback = function (error) {
        if (!error) {
          this.userBarView.render();
        }
      }.bind(this);

      this.userCollection.whoAmI(callback);
      // this.statisticBarView.render($("#statisticBar"))

      if (this.renderFirst) {
        this.renderFirst = false;

        this.selectMenuItem();

        $('.arangodbLogo').on('click', function () {
          self.selectMenuItem();
        });

        $('#dbStatus').on('click', function () {
          self.changeDB();
        });
      }

      self.resize();

      if (window.frontendConfig.isEnterprise === true) {
        $('#ArangoDBLogo').after('<span id="enterpriseLabel" style="display: none">Enterprise Edition</span>');
        $('#enterpriseLabel').fadeIn('slow');
      } else {
        $('#ArangoDBLogo').after('<span id="communityLabel" style="display: none">Community Edition</span>');
        $('#communityLabel').fadeIn('slow');
        $('.enterprise-menu').show();
      }

      return this;
    },

    resize: function () {
      // set menu sizes - responsive
      var height = $(window).height() - $('.subMenuEntries').first().height();
      $('#navigationBar').css('min-height', height);
      $('#navigationBar').css('height', height);
    },

    navigateBySelect: function () {
      var navigateTo = $('#arangoCollectionSelect').find('option:selected').val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    handleKeyboardHotkeys: function () {
      arangoHelper.enableKeyboardHotkeys(true);
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      var dropdown = false;

      if (navigateTo === 'enterprise') {
        return;
      }

      if ($(tab).hasClass('fa')) {
        return;
      }

      if (navigateTo === '') {
        navigateTo = $(tab).attr('class');
      }

      if (navigateTo === 'links') {
        dropdown = true;
        $('#link_dropdown').slideToggle(1);
        e.preventDefault();
      } else if (navigateTo === 'tools') {
        dropdown = true;
        $('#tools_dropdown').slideToggle(1);
        e.preventDefault();
      } else if (navigateTo === 'dbselection') {
        dropdown = true;
        $('#dbs_dropdown').slideToggle(1);
        e.preventDefault();
      }

      if (!dropdown) {
        window.App.navigate(navigateTo, {trigger: true});
        e.preventDefault();
      }
    },

    handleSelectNavigation: function () {
      var self = this;
      $('#arangoCollectionSelect').change(function () {
        self.navigateBySelect();
      });
    },

    subViewConfig: {
      documents: 'collections',
      collection: 'collections'
    },

    subMenuConfig: {
      cluster: [
        {
          name: 'Dashboard',
          view: undefined,
          active: true
        }
      ],
      collections: [
        {
          name: '',
          view: undefined,
          active: false
        }
      ],
      queries: [
        {
          name: 'Editor',
          route: 'query',
          active: true
        },
        {
          name: 'Running Queries',
          route: 'queryManagement',
          params: {
            active: true
          },
          active: undefined
        },
        {
          name: 'Slow Query History',
          route: 'queryManagement',
          params: {
            active: false
          },
          active: undefined
        }
      ]
    },

    renderSubMenu: function (id) {
      var self = this;

      if (id === undefined) {
        if (window.isCluster) {
          id = 'cluster';
        } else {
          id = 'dashboard';
        }
      }

      if (this.subMenuConfig[id]) {
        $(this.subEl + ' .bottom').html('');
        var cssclass = '';

        _.each(this.subMenuConfig[id], function (menu) {
          if (menu.active) {
            cssclass = 'active';
          } else {
            cssclass = '';
          }
          if (menu.disabled) {
            cssclass = 'disabled';
          }

          $(self.subEl + ' .bottom').append(
            '<li class="subMenuEntry ' + cssclass + '"><a>' + menu.name + '</a></li>'
          );
          if (!menu.disabled) {
            $(self.subEl + ' .bottom').children().last().bind('click', function (elem) {
              $('#subNavigationBar .breadcrumb').html('');
              self.activeSubMenu = menu;
              self.renderSubView(menu, elem);
            });
          }
        });
      } else {
        $(self.subEl + ' .bottom').append(
          '<li class="subMenuEntry</li>'
        );
      }
    },

    renderSubView: function (menu, element) {
      // trigger routers route
      if (window.App[menu.route]) {
        if (window.App[menu.route].resetState) {
          window.App[menu.route].resetState();
        }
        window.App[menu.route]();
      }

      // select active sub view entry
      $(this.subEl + ' .bottom').children().removeClass('active');
      $(element.currentTarget).addClass('active');
    },

    switchTab: function (e) {
      var id = $(e.currentTarget).children().first().attr('id');

      if (id === 'enterprise') {
        window.open('https://www.arangodb.com/download-arangodb-enterprise/', '_blank');
        return;
      }

      if (id) {
        this.selectMenuItem(id + '-menu');
      }
    },

    /*
    breadcrumb: function (name) {

      if (window.location.hash.split('/')[0] !== '#collection') {
        $('#subNavigationBar .breadcrumb').html(
          '<a class="activeBread" href="#' + name + '">' + name + '</a>'
        )
      }

    },
    */

    selectMenuItem: function (menuItem, noMenuEntry) {
      if (menuItem === undefined) {
        menuItem = window.location.hash.split('/')[0];
        menuItem = menuItem.substr(1, menuItem.length - 1);
      }

      // Location for selecting MainView Primary Navigaation Entry
      if (menuItem === '') {
        if (window.App.isCluster) {
          menuItem = 'cluster';
        } else {
          menuItem = 'dashboard';
        }
      } else if (menuItem === 'cNodes' || menuItem === 'dNodes') {
        menuItem = 'nodes';
      }
      try {
        this.renderSubMenu(menuItem.split('-')[0]);
      } catch (e) {
        this.renderSubMenu(menuItem);
      }

      // this.breadcrumb(menuItem.split('-')[0])

      $('.navlist li').removeClass('active');
      if (typeof menuItem === 'string') {
        if (noMenuEntry) {
          $('.' + this.subViewConfig[menuItem] + '-menu').addClass('active');
        } else if (menuItem) {
          $('.' + menuItem).addClass('active');
          $('.' + menuItem + '-menu').addClass('active');
        }
      }
      arangoHelper.hideArangoNotifications();
    },

    showSubDropdown: function (e) {
      $(e.currentTarget).find('.subBarDropdown').toggle();
    },

    showDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === 'links' || navigateTo === 'link_dropdown' || e.currentTarget.id === 'links') {
        $('#link_dropdown').fadeIn(1);
      } else if (navigateTo === 'tools' || navigateTo === 'tools_dropdown' || e.currentTarget.id === 'tools') {
        $('#tools_dropdown').fadeIn(1);
      } else if (navigateTo === 'dbselection' || navigateTo === 'dbs_dropdown' || e.currentTarget.id === 'dbselection') {
        $('#dbs_dropdown').fadeIn(1);
      }
    },

    hideDropdown: function (e) {
      // var tab = e.target || e.srcElement;
      // tab = $(tab).parent();
      $('#link_dropdown').fadeOut(1);
      $('#tools_dropdown').fadeOut(1);
      $('#dbs_dropdown').fadeOut(1);
    }

  });
}());
