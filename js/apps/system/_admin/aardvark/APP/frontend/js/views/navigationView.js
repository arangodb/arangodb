/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, window, arangoHelper, _*/
(function () {
  "use strict";
  window.NavigationView = Backbone.View.extend({
    el: '#navigationBar',
    subEl: '#subNavigationBar',

    events: {
      "change #arangoCollectionSelect": "navigateBySelect",
      "click .tab": "navigateByTab",
      "click li": "switchTab",
      "click .arangodbLogo": "selectMenuItem",
      "mouseenter .dropdown > *": "showDropdown",
      "mouseleave .dropdown": "hideDropdown"
    },

    renderFirst: true,
    activeSubMenu: undefined,

    initialize: function () {

      this.userCollection = this.options.userCollection;
      this.currentDB = this.options.currentDB;
      this.dbSelectionView = new window.DBSelectionView({
        collection: this.options.database,
        current: this.currentDB
      });
      this.userBarView = new window.UserBarView({
        userCollection: this.userCollection
      });
      this.notificationView = new window.NotificationView({
        collection: this.options.notificationCollection
      });
      this.statisticBarView = new window.StatisticBarView({
          currentDB: this.currentDB
      });
      this.handleKeyboardHotkeys();
    },

    handleSelectDatabase: function () {
      this.dbSelectionView.render($("#dbSelect"));
    },

    template: templateEngine.createTemplate("navigationView.ejs"),
    templateSub: templateEngine.createTemplate("subNavigationView.ejs"),

    render: function () {
      var self = this;

      $(this.el).html(this.template.render({
        currentDB: this.currentDB,
        isCluster: window.App.isCluster
      }));

      $(this.subEl).html(this.templateSub.render({}));
      
      this.dbSelectionView.render($("#dbSelect"));
      this.notificationView.render($("#notificationBar"));

      var callback = function(error) {
        if (!error) {
          this.userBarView.render();
        }
      }.bind(this);

      this.userCollection.whoAmI(callback);
      this.statisticBarView.render($("#statisticBar"));

      if (this.renderFirst) {
        this.renderFirst = false;
          
        var select = ((window.location.hash).substr(1, (window.location.hash).length) + '-menu');
        if (select.indexOf('/') === -1) {
          this.selectMenuItem(select);
        }
        else {
          //handle subview menus which do not have a main menu entry
          if (select.split('/')[2] === 'documents') {
            self.selectMenuItem(select.split('/')[2], true);
          }
          else if (select.split('/')[0] === 'collection') {
            self.selectMenuItem(select.split('/')[0], true);
          }
        }

        $('.arangodbLogo').on('click', function() {
          self.selectMenuItem();
        });
      }

      return this;
    },

    navigateBySelect: function () {
      var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    handleKeyboardHotkeys: function () {
      arangoHelper.enableKeyboardHotkeys(true);
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement,
      navigateTo = tab.id,
      dropdown = false;

      if (navigateTo === "") {
        navigateTo = $(tab).attr("class");
      }
      
      if (navigateTo === "links") {
        dropdown = true;
        $("#link_dropdown").slideToggle(1);
        e.preventDefault();
      }
      else if (navigateTo === "tools") {
        dropdown = true;
        $("#tools_dropdown").slideToggle(1);
        e.preventDefault();
      }
      else if (navigateTo === "dbselection") {
        dropdown = true;
        $("#dbs_dropdown").slideToggle(1);
        e.preventDefault();
      }

      if (!dropdown) {
        window.App.navigate(navigateTo, {trigger: true});
        e.preventDefault();
      }
    },

    handleSelectNavigation: function () {
      var self = this;
      $("#arangoCollectionSelect").change(function() {
        self.navigateBySelect();
      });
    },

    subViewConfig: {
      documents: 'collections',
      collection: 'collections'
    },

    subMenuConfig: {
      collection: [
        {
          name: 'Settings',
          view: undefined
        },
        {
          name: 'Indices',
          view: undefined
        },
        {
          name: 'Content',
          view: undefined,
          active: true
        }
      ],
      cluster: [
        {
          name: 'Dashboard',
          view: undefined
        },
        {
          name: 'Logs',
          view: undefined
        }
      ],
      query: [
        {
          name: 'Slow Query History',
          route: 'queryManagement',
          params: {
            active: false
          },
          active: undefined
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
          name: 'Editor',
          route: 'query2',
          active: true
        }
      ]
    },

    renderSubMenu: function(id) {
      var self = this;

      if (id === undefined) {
        if (window.isCluster) {
          id = 'cluster';
        }
        else {
          id = 'dashboard';
        }
      }

      $(this.subEl + ' .right').html('');
      var cssclass = "";

      _.each(this.subMenuConfig[id], function(menu) {

        if (menu.active) {
          cssclass = 'active';
        }
        else {
          cssclass = '';
        }

        $(self.subEl +  ' .right').append(
          '<li class="subMenuEntry ' + cssclass + '"><a>' + menu.name + '</a></li>'
        );
        $(self.subEl + ' .right').children().last().bind('click', function(elem) {
          self.activeSubMenu = menu;
          self.renderSubView(menu, elem);
        });
      });
    },

    renderSubView: function(menu, element) {
      //trigger routers route
      if (window.App[menu.route]) {
        window.App[menu.route]("asd");
      }

      //select active sub view entry
      $(this.subEl + ' .right').children().removeClass('active');
      $(element.currentTarget).addClass('active');
    },

    switchTab: function(e) {
      var id = $(e.currentTarget).children().first().attr('id');

      if (id) {
        this.selectMenuItem(id + '-menu');
      }
    },

    selectMenuItem: function (menuItem, noMenuEntry) {
      try {
        this.renderSubMenu(menuItem.split('-')[0]);
      }
      catch (e) {
        this.renderSubMenu(menuItem);
      }
      $('.navlist li').removeClass('active');
      if (typeof menuItem === 'string') {
        if (noMenuEntry) {
          $('.' + this.subViewConfig[menuItem] + '-menu').addClass('active');
        }
        else if (menuItem) {
          $('.' + menuItem).addClass('active');
        }
      }
      arangoHelper.hideArangoNotifications();
    },

    showDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "links" || navigateTo === "link_dropdown" || e.currentTarget.id === 'links') {
        $("#link_dropdown").fadeIn(1);
      }
      else if (navigateTo === "tools" || navigateTo === "tools_dropdown" || e.currentTarget.id === 'tools') {
        $("#tools_dropdown").fadeIn(1);
      }
      else if (navigateTo === "dbselection" || navigateTo === "dbs_dropdown" || e.currentTarget.id === 'dbselection') {
        $("#dbs_dropdown").fadeIn(1);
      }
    },

    hideDropdown: function (e) {
      var tab = e.target || e.srcElement;
      tab = $(tab).parent();
      $("#link_dropdown").fadeOut(1);
      $("#tools_dropdown").fadeOut(1);
      $("#dbs_dropdown").fadeOut(1);
    }

  });
}());
