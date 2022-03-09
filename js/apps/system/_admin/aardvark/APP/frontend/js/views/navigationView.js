/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, templateEngine, $, window, sessionStorage, Storage, arangoHelper, _ */
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
      'click .shortcut-icons p': 'showShortcutModal'
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
      this.statisticsInAllDatabases = options.statisticsInAllDatabases;

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
        frontendConfig: window.frontendConfig,
        currentDB: this.currentDB.toJSON()
      }));
      arangoHelper.checkDatabasePermissions(this.continueRender.bind(this), this.continueRender.bind(this));
      if (window.frontendConfig.isEnterprise === true) {
        this.fetchServerTime();
      }
    },

    continueRender: function (readOnly) {
      var self = this;

      $(this.el).html(this.template.render({
        currentDB: this.currentDB,
        statisticsInAllDatabases: this.statisticsInAllDatabases,
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

    fetchLicenseInfo: function (serverTime) {
      const self = this;
      const url = arangoHelper.databaseUrl('/_admin/license');

      $.ajax({
        type: "GET",
        url: url,
        success: function (licenseData) {
          if (licenseData.status && licenseData.features && licenseData.features.expires) {
            self.renderLicenseInfo(licenseData.status, licenseData.features.expires, serverTime);
          } else {
            self.showLicenseError();
          }
        },
        error: function () {
          self.showLicenseError();
        }
      });
    },

    fetchServerTime: function () {
      const self = this;
      const url = arangoHelper.databaseUrl('/_admin/time');

      $.ajax({
        type: "GET",
        url: url,
        success: function (timeData) {
          if (!timeData.error && timeData.code === 200 && timeData.time) {
            self.fetchLicenseInfo(timeData.time);
          } else {
            self.showGetTimeError();
          }
        },
        error: function () {
          self.showGetTimeError();
        }
      });
    },

    showLicenseError: function () {
      const errorElement = '<div id="subNavLicenseInfo" class="alert alert-danger alert-license"><span><i class="fa fa-exclamation-triangle"></i></span> <span id="licenseInfoText">Error: Failed to fetch license information</span></div>';
      $('#licenseInfoArea').append(errorElement);
    },

    showGetTimeError: function () {
      const errorElement = '<div id="subNavLicenseInfo" class="alert alert-danger alert-license"><span><i class="fa fa-exclamation-triangle"></i></span> <span id="licenseInfoText">Error: Failed to fetch server time</span></div>';
      $('#licenseInfoArea').append(errorElement);
    },

    renderLicenseInfo: function (status, expires, serverTime) {
      if (status !== null && expires !== null) {
        let infotext = '';
        let daysInfo = '';
        let alertClasses = 'alert alert-license';
        switch (status) {
        case 'expiring':
          let remains = expires - Math.round(serverTime);
          let daysInfo = Math.ceil(remains / (3600*24));
          let hoursInfo = '';
          let minutesInfo = '';
          infotext = 'Your license is expiring in under ';
          if (daysInfo > 1) {
            infotext += daysInfo + ' days';
          } else {
            hoursInfo = Math.ceil(remains / 3600);
            if (hoursInfo > 1) {
              infotext += hoursInfo + ' hours';
            } else {
              minutesInfo = Math.ceil(remains / 60);
              infotext += minutesInfo + ' minutes';
            }

          }
          infotext += '. Please contact ArangoDB sales to extend your license urgently.';
          this.appendLicenseInfoToUi(infotext, alertClasses);
          break;
        case 'expired':
          daysInfo = Math.floor((Math.round(serverTime) - expires) / (3600*24));
          infotext = 'Your license expired ' + daysInfo + ' days ago. New enterprise features cannot be created. Please contact ArangoDB sales immediately.';
          alertClasses += ' alert-danger';
          this.appendLicenseInfoToUi(infotext, alertClasses);
          break;
        case 'read-only':
          infotext = 'Your license has expired. This installation has been restricted to read-only mode. Please contact ArangoDB sales immediately to extend your license.';
          alertClasses += ' alert-danger';
          this.appendLicenseInfoToUi(infotext, alertClasses);
          break;
        default:
          break;
        }
      }
    },

    appendLicenseInfoToUi: function(infotext, alertClasses) {
      var infoElement = '<div id="subNavLicenseInfo" class="' + alertClasses + '"><span><i class="fa fa-exclamation-triangle"></i></span> <span id="licenseInfoText">' + infotext + '</span></div>';
      $('#licenseInfoArea').append(infoElement);
    },

    resize: function () {
      // set menu sizes - responsive
      const height = $(window).height() - $('.subMenuEntries').first().height();
      const navBar = $('#navigationBar');
      navBar.css('min-height', height);
      navBar.css('height', height);
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
          '<li class="subMenuEntry"></li>'
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
      } else if (menu.href) {
        window.App.navigate(menu.href, {trigger: true});
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
    }

  });
}());
