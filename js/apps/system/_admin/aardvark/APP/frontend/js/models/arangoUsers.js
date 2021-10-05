/* jshint strict: false */
/* global Backbone, $, window, arangoHelper */
window.Users = Backbone.Model.extend({
  defaults: {
    user: '',
    active: false,
    extra: {}
  },

  idAttribute: 'user',

  parse: function (d) {
    this.isNotNew = true;
    return d;
  },

  isNew: function () {
    return !this.isNotNew;
  },

  url: function () {
    if (!this.isNew() && this.get('user') !== '') {
      return arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.get('user')));
    }
    return arangoHelper.databaseUrl('/_api/user');
  },
  
  setPassword: function (passwd) {
    $.ajax({
      cache: false,
      type: 'PATCH',
      url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.get('user'))),
      data: JSON.stringify({ passwd: passwd }),
      contentType: 'application/json',
      processData: false
    });
  },

  setExtras: function (name, img, callback) {
    $.ajax({
      cache: false,
      type: 'PATCH',
      url: arangoHelper.databaseUrl('/_api/user/' + encodeURIComponent(this.get('user'))),
      data: JSON.stringify({'extra': {'name': name, 'img': img}}),
      contentType: 'application/json',
      processData: false,
      success: function (data) {
        callback(false, data);
      },
      error: function () {
        callback(true);
      }
    });
  }

});
