/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, templateEngine, $, window, document, sessionStorage, Storage, arangoHelper, frontendConfig, Noty, _ */
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
    isOffline: true,
    isOfflineCounter: 0,
    firstLogin: true,
    timer: 15000,
    lap: 0,
    reconnectTimeout: null,
    // last time of JWT renewal call, in seconds.
    // this is initialized to the current time so we don't 
    // fire off a renewal request at the very beginning.
    lastTokenRenewal: Date.now() / 1000, 

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

      // also server online check
      window.setInterval(() => {
        this.getVersion();
      }, this.timer);
      this.getVersion();

      window.VISIBLE = true;
      document.addEventListener('visibilitychange', () => {
        window.VISIBLE = !window.VISIBLE;
      });

      $('#offlinePlaceholder button').on('click', () => {
        this.getVersion();
      });

      window.setTimeout(() => {
        if (window.frontendConfig.isCluster === true) {
          $('.health-state').css('cursor', 'pointer');
          $('.health-state').on('click', () => {
            window.App.navigate('#nodes', {trigger: true});
          });
        }
      }, 1000);

      // track an activity once when we initialize this view
      arangoHelper.noteActivity();
      
      window.setInterval(() => {
        if (this.isOffline) {
          // only try to renew token if we are still online
          return;
        }
        
        var frac = (frontendConfig.sessionTimeout >= 1800) ? 0.95 : 0.8;
        // threshold for renewal: once session is x% over
        var renewalThreshold = frontendConfig.sessionTimeout * frac;

        var now = Date.now() / 1000;
        var lastActivity = arangoHelper.lastActivity();

        // seconds in which the last user activity counts as significant
        var lastSignificantActivityTimePeriod = 90 * 60;

        // if this is more than the renewal threshold, limit it
        if (lastSignificantActivityTimePeriod > renewalThreshold * 0.95) {
          lastSignificantActivityTimePeriod = renewalThreshold * 0.95;
        }

        if (lastActivity > 0 && (now - lastActivity) > lastSignificantActivityTimePeriod) {
          // don't make an attempt to renew the token if last 
          // user activity is longer than 90 minutes ago
          return;
        }

        // to save some superfluous HTTP requests to the server, 
        // try to renew only if session time is x% or more over
        if (now - this.lastTokenRenewal < renewalThreshold) {
          return;
        }

        arangoHelper.renewJwt(() => {
          // successful renewal of token. now store last renewal time so
          // that we later only renew if the session is again about to
          // expire
          this.lastTokenRenewal = now;
        });
      }, 15 * 1000);
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
        $('.enterprise-menu').hide();
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
        }
        // intentionally no error handling: non-root users may not be allowed to fetch license information, but in that case we do not want to show an error
        // Additional note: This is required if "--server.harden true" is being set.
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
        }
        // intentionally no error handling: non-root users may not be allowed to fetch license information, but in that case we do not want to show an error
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
      var link = $(e.currentTarget).children().first();
      var id = link.attr('id');

      if (id === 'enterprise') {
        e.preventDefault();
        window.open(link.attr('href'), '_blank');
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
      } else if (menuItem === 'view') {
        menuItem = 'views';
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
    },

    showServerStatus: function (isOnline) {
      if (!window.App.isCluster) {
        if (isOnline === true) {
          $('#healthStatus').removeClass('negative');
          $('#healthStatus').addClass('positive');
          $('.health-state').html('GOOD');
          $('.health-icon').html('<i class="fa fa-check-circle"></i>');
          $('#offlinePlaceholder').hide();
        } else {
          $('#healthStatus').removeClass('positive');
          $('#healthStatus').addClass('negative');
          $('.health-state').html('UNKNOWN');
          $('.health-icon').html('<i class="fa fa-exclamation-circle"></i>');

          // remove modals if visible
          window.modalView.hide();

          // show offline overlay
          $('#offlinePlaceholder').show();

          // remove error messages
          Noty.clearQueue();
          Noty.closeAll();

          this.reconnectAnimation(0);
        }
      } else {
        this.renderClusterState(isOnline);
      }
    },

    reconnectAnimation: function (lap) {
      if (lap === 0) {
        this.lap = lap;
        $('#offlineSeconds').text(this.timer / 1000);
        clearTimeout(this.reconnectTimeout);
      }

      if (this.lap < this.timer / 1000) {
        this.lap++;
        $('#offlineSeconds').text(this.timer / 1000 - this.lap);

        this.reconnectTimeout = window.setTimeout(() => {
          if (this.timer / 1000 - this.lap === 0) {
            this.getVersion();
          } else {
            this.reconnectAnimation(this.lap);
          }
        }, 1000);
      }
    },

    renderClusterState: function (connection) {
      if (connection) {
        $('#offlinePlaceholder').hide();

        var callbackFunction = (data) => {
          window.clusterHealth = data.Health;

          var error = 0;

          if (Object.keys(window.clusterHealth).length !== 0) {
            _.each(window.clusterHealth, (node) => {
              if (node.Role === 'DBServer' || node.Role === 'Coordinator') {
                if (node.Status !== 'GOOD') {
                  error++;
                }
              }
            });

            if (error > 0) {
              $('#healthStatus').removeClass('positive');
              $('#healthStatus').addClass('negative');
              $('.health-state').html(error + ' NODE(S) ERROR');
              $('.health-icon').html('<i class="fa fa-exclamation-circle"></i>');
            } else {
              $('#healthStatus').removeClass('negative');
              $('#healthStatus').addClass('positive');
              $('.health-state').html('NODES OK');
              $('.health-icon').html('<i class="fa fa-check-circle"></i>');
            }
          } else {
            $('.health-state').html('HEALTH ERROR');
            $('#healthStatus').removeClass('positive');
            $('#healthStatus').addClass('negative');
            $('.health-icon').html('<i class="fa fa-exclamation-circle"></i>');
          }
        };

        if (frontendConfig.clusterApiJwtPolicy !== 'jwt-all') {
          // check cluster state
          $.ajax({
            type: 'GET',
            cache: false,
            url: arangoHelper.databaseUrl('/_admin/cluster/health'),
            contentType: 'application/json',
            processData: false,
            async: true,
            success: (data) => {
              if (window.App) {
                window.App.lastHealthCheckResult = data;
              }
              callbackFunction(data);
              // notify NodesView about new health data
              if (window.location.hash === '#nodes' && window.App && window.App.nodesView) {
                window.App.nodesView.render(false);
              }
            },
            error: () => {
              if (window.App) {
                window.App.lastHealthCheckResult = null;
              }
            }
          });
        }
      } else {
        $('#healthStatus').removeClass('positive');
        $('#healthStatus').addClass('negative');
        $('.health-state').html(window.location.host + ' OFFLINE');
        $('.health-icon').html('<i class="fa fa-exclamation-circle"></i>');

        // show offline overlay
        $('#offlinePlaceholder').show();
        this.reconnectAnimation(0);
      }
    },

    getVersion: function () {
      // always retry this call, because it also checks if the server is online
      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/version'),
        contentType: 'application/json',
        processData: false,
        async: true,
        success: (data) => {
          frontendConfig.version = data;
          if (!frontendConfig.version.hasOwnProperty('version')) {
            frontendConfig.version.version = 'N/A';
          }
          this.showServerStatus(true);
          if (this.isOffline === true) {
            this.isOffline = false;
            this.isOfflineCounter = 0;
            if (!this.firstLogin) {
              window.setTimeout(() => {
                this.showServerStatus(true);
              }, 1000);
            } else {
              this.firstLogin = false;
            }
          }
        },
        error: (jqXHR) => {
          if (jqXHR.status === 401) {
            this.showServerStatus(true);
            window.App.navigate('login', {trigger: true});
          } else {
            this.isOffline = true;
            this.isOfflineCounter++;
            if (this.isOfflineCounter >= 1) {
              // arangoHelper.arangoError("Server", "Server is offline")
              this.showServerStatus(false);
            }
          }
        }
      });
    }

  });
}());
