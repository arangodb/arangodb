var app = app || {};

$(function () {
	'use strict';

	app.EditView = app.EditSuperView.extend({
    
    save: function() {
      var obj = this.parse();
      this.model.save(obj);
      $('#document_modal').modal('hide');
    },
    
		render: function () {
      this.superRender({model: this.model, columns: this.columns});
			return this;
		}
	});
}());
