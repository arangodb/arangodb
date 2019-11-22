/* jshint browser: true */
/* global marked, Backbone, $, window, arangoHelper, templateEngine, _ */
(function () {
  'use strict';

  window.StoreDetailView = Backbone.View.extend({
    el: '#content',

    divs: ['#readme', '#swagger', '#app-info', '#sideinformation', '#information', '#settings'],
    navs: ['#service-info', '#service-api', '#service-readme', '#service-settings'],

    template: templateEngine.createTemplate('storeDetailView.ejs'),

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
      'click #installService': 'installService'
    },

    installService: function () {
      arangoHelper.createMountPointModal(this.installFoxxFromStore.bind(this));
    },

    installCallback: function (result) {
      window.App.navigate('#services', {trigger: true});
      window.App.applicationsView.installCallback(result);
    },

    installFoxxFromStore: function (e) {
      if (window.modalView.modalTestAll()) {
        var mount, info, options;
        if (this._upgrade) {
          mount = this.mount;
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
          if (mount.charAt(0) !== '/') {
            mount = '/' + mount;
          }
        }

        info = {
          name: this.model.get('name'),
          version: this.model.get('latestVersion')
        };

        options = arangoHelper.getFoxxFlags();
        this.collection.install('store', info, mount, options, this.installCallback.bind(this));

        window.modalView.hide();
        arangoHelper.arangoNotification('Services', 'Installing ' + this.model.get('name') + '.');
      }
    },

    resize: function (auto) {
      if (auto) {
        $('.innerContent').css('height', 'auto');
      } else {
        $('.innerContent').height($('.centralRow').height() - 150);
      }
    },

    render: function () {
      var self = this;

      this.model.fetchThumbnail(function (imgUrl) {
        var callback = function (error, db) {
          if (error) {
            arangoHelper.arangoError('DB', 'Could not get current database');
          } else {
            var thumbnailUrl = this.model.get('defaultThumbnailUrl');
            if (this.model.get('manifest')) {
              try {
                if (this.model.get('manifest').thumbnail) {
                  thumbnailUrl = this.model.getThumbnailUrl();
                }
              } catch (ignore) {
              }
            }
            $(this.el).html(this.template.render({
              app: this.model,
              baseUrl: arangoHelper.databaseUrl('', db),
              installed: true,
              image: thumbnailUrl
            }));
            this.model.fetchReadme(self.renderReadme);
          }

          this.breadcrumb();
        }.bind(this);

        arangoHelper.currentDatabase(callback);

        if (_.isEmpty(this.model.get('config'))) {
          $('#service-settings').attr('disabled', true);
        }
      }.bind(this));
      this.resize(false);
      return $(this.el);
    },

    renderReadme: function (readme) {
      var html = marked(readme);
      $('#readme').html(html);
    },

    breadcrumb: function () {
      var string = 'Service: ' + this.model.get('name') + '<i class="fa fa-ellipsis-v" aria-hidden="true"></i>';

      var contributors = '<p class="mount"><span>Contributors:</span>';
      if (this.model.get('contributors') && this.model.get('contributors').length > 0) {
        _.each(this.model.get('contributors'), function (contributor) {
          if (contributor.email) {
            contributors += '<a href="mailto:' + contributor.email + '">' + (contributor.name || contributor.email) + '</a>';
          } else if (contributor.name) {
            contributors += '<a>contributor.name</a>';
          }
        });
      } else {
        contributors += 'No contributors';
      }
      contributors += '</p>';
      $('.information').append(
        contributors
      );

      // information box info tab
      if (this.model.get('author')) {
        $('.information').append(
          '<p class="mount"><span>Author:</span>' + this.model.get('author') + '</p>'
        );
      }
      if (this.model.get('mount')) {
        $('.information').append(
          '<p class="mount"><span>Mount:</span>' + this.model.get('mount') + '</p>'
        );
      }
      if (this.model.get('development')) {
        if (this.model.get('path')) {
          $('.information').append(
            '<p class="path"><span>Path:</span>' + this.model.get('path') + '</p>'
          );
        }
      }
      $('#subNavigationBar .breadcrumb').html(string);
    },

    storeUrl: function (currentDB) {
      return arangoHelper.databaseUrl(this.model.get('mount'), currentDB);
    }

  });
}());
