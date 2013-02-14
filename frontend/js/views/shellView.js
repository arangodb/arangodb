var shellView = Backbone.View.extend({
  el: '#content',
  shArray: [],
  initialize: function () {
    this.autocomplete();
  },
  events: {
    'click .reloadicon'       : 'reloadShell',
    'click #submitShellButton': 'submitShell',
    'submit form'             : 'submitShell',
    'click #clearShellButton' : 'clearShell',
    'click .clearicon'        : 'clearShell'
   },

  template: new EJS({url: '/_admin/html/js/templates/shellView.ejs'}),

  render: function() {
    $(this.el).html(this.template.text);
    this.drawWelcomeMsg();
    return this;
  },
  autocomplete: function () {
    self = this;
    $("#avocshContent").autocomplete({
      source: self.shArray
    });
  },
  submitShell: function () {
    var varInput = "#shellInput";
    var varContent = "#shellContent";
    var data = $(varInput).val();

    var r = [ ];
    for (var i = 0; i < this.shArray.length; ++i) {
      if (this.shArray[i] != data) {
        r.push(this.shArray[i]);
      }
    }

    this.shArray = r;
    if (this.shArray.length > 4) {
      this.shArray.shift();
    }
    this.shArray.push(data);

    $("#shellInput").autocomplete({
      source: this.shArray
    });

    if (data == "exit") {
      location.reload();
      return;
    }

    var command;
    if (data == "help") {
      command = "require(\"arangosh\").HELP";
    }
    else if (data == "reset") {
      command = "$('#shellContent').html(\"\");undefined;";
    }
    else {
      command = data;
    }

    var client = "arangosh> " + escapeHTML(data) + "<br>";
    $(varContent).append('<b class="shellClient">' + client + '</b>');
    this.evaloutput(command);
    console.log(command);
    $(varContent).animate({scrollTop:$(varContent)[0].scrollHeight}, 1);
    $(varInput).val('');
    return false;
  },
  clearShell: function () {
    $('#shellContent').html("");
  },
  reloadShell: function () {
    db._collections();
    $('#shellContent').html("");
    this.drawWelcomeMsg();
    $('#shellContent').append("<p> --- Reloaded ---</p>");

  },
  drawWelcomeMsg: function () {
    if ($('#shellContent').text().length == 0 ) {
      $('#shellContent').append('<img src="img/arangodblogo.png" style="width:141px; height:24px; margin-left:0;"></img>');
      print("Welcome to arangosh Copyright (c) 2012 triAGENS GmbH.");
      print(require("arangosh").HELP);
      start_pretty_print();
    }
  },
  evaloutput: function (data) {
    try {
      var result = eval(data); 
      if (result !== undefined) {
        print(result);
      }
    }
    catch(e) {
      $('#shellContent').append('<p class="shellError">Error:' + e + '</p>');
    }
  }

});
