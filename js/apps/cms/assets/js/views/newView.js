var app = app || {};

$(function () {
	'use strict';

	app.NewView = app.EditSuperView.extend({

    save: function() {
      var obj = this.parse();
      this.collection.create(obj);
      $('#document_modal').modal('hide');
    },
    
		render: function () {
      this.superRender({columns: this.columns});
      return this;
		}
	});
}());
