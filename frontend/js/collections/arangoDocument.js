window.arangoDocument = Backbone.Collection.extend({
  url: '/_api/document/',
  model: arangoDocument,

  getDocument: function (colid, docid, view) {
    this.clearDocument();
    var self = this;
    $.ajax({
      type: "GET",
      url: "/_api/document/" + colid +"/"+ docid,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        window.arangoDocumentStore.add(data);

        if (view == "source") {
          window.documentSourceView.fillSourceBox();
        }
        else {
          window.documentView.initTable();
          window.documentView.drawTable();
        }
      },
      error: function(data) {
      }
    });
  },

  saveDocument: function (view) {
    if (view == "source") {
      var model = $('#documentSourceBox').val();
      var tmp1 = window.location.hash.split("/")[2];
      var tmp2 = window.location.hash.split("/")[1];
      var docID = tmp2 + "/" + tmp1;
    }
    else {
      var tmp = this.models[0].attributes;
      var model = JSON.stringify(tmp);
      var docID = this.models[0].attributes._id;
    }


    $.ajax({
      type: "PUT",
      url: "/_api/document/" + docID,
      data: model,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        console.log("saved");
      },
      error: function(data) {
        //alert(getErrorMessage(data));
        console.log(data);
      }
    });

  },

  updateLocalDocument: function (data) {
    this.clearDocument();
    this.add(data);
  },
  clearDocument: function () {
    window.arangoDocumentStore.reset();
  }

});
