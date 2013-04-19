var dashboardView = Backbone.View.extend({
  el: '#content',
  updateInterval: 3000,

  initialize: function () {
    //notes
    //this.collection.fetch();
    //this.options.description.fetch();
    var self = this;

    this.collection.fetch({
      success: function() {
        window.setInterval(function() {
          self.updateSystems();
          self.collection.fetch();
        }, self.updateInterval);
      }
    });


  },

  template: new EJS({url: 'js/templates/dashboardView.ejs'}),

  render: function() {
    var self = this;
    $(this.el).html(this.template.text);

    $.each(this.options.description.models[0].attributes.groups, function(key, val) {
      $('#content').append(
        '<div class="statGroups" id="'+this.group+'">' +
          '<h4>'+this.name+'</h4>' +
        '</div>');
    });
    $.each(this.options.description.models[0].attributes.figures, function(key, val) {
      if (this.group === 'system') {
        self.renderSystem(this.identifier, this.name, this.description, this.type, this.units);
      }
      else if (this.group === 'client') {
        self.renderClient(this.identifier, this.name, this.description, this.type, this.units);
      }
    });

    return this;
  },
  renderClient: function (identifier, name, desc, type, units) {
    $('#client').append(
      '<div class="statClient" id="'+identifier+'">' +
        '<h5>' + name + '</h5>' +
      '</div>'
    );
  },
  renderSystem: function (identifier, name, desc, type, units) {
    $('#system').append(
      '<div class="statSystem" id="'+identifier+'">' +
        '<table><tr>' +
          '<th>'+ name +'</th>' +
          '<th class="updateValue">'+ 'counting...' +'</th>' +
        '</tr></table>' +
      '</div>'
    );
  },
  updateClient: function (identifier, count, counts) {

  },
  updateSystems: function () {
    $.each(this.collection.models[0].attributes.system, function(key, val) {
      $('#'+key+' .updateValue').html(val);
    });

  }

});
