///////////////////////////////////////////////////////////////////////////////
/// master.js 
/// avocadodb js api  
///////////////////////////////////////////////////////////////////////////////

// documents global vars
var collectionCount;
var totalCollectionCount;
var collectionCurrentPage;  
var globalCollectionName;  

$(document).ready(function() {       

showCursor();
///////////////////////////////////////////////////////////////////////////////
/// global variables 
///////////////////////////////////////////////////////////////////////////////
var open = false;
var tableView = true;
var sid = ($.cookie("sid")); 
var currentUser; 
//logtable vars
var currentPage = 1; 
var currentAmount; 
var currentTableID = "#logTableID"; 
var currentLoglevel = 5;  
//live click for all log tables 

var tables = ["#logTableID", "#critLogTableID", "#warnLogTableID", "#infoLogTableID", "#debugLogTableID"];

$.each(tables, function(v, i ) {
  $(i + '_next').live('click', function () {

    if ( i == "#logTableID" ) {
      createNextPagination("all");  
    }
    else {
      createNextPagination();  
    }
  });
  $(i + '_prev').live('click', function () {
    if ( i == "#logTableID" ) {
      createPrevPagination("all");  
    }
    else {
      createPrevPagination();  
    }
  });
  $(i + '_first').live('click', function () {
    createLogTable(currentLoglevel);
  });
  $(i+ '_last').live('click', function () {
    createLastLogPagination(i); 
    
  });
});


$("#documents_prev").live('click', function () {
  createPrevDocPagination();
});

$("#documents_next").live('click', function () {
  createNextDocPagination();
});

$("#documents_first").live('click', function () {
  createFirstPagination("#documentsTable"); 
});

$("#documents_last").live('click', function () {
  createLastPagination("#documentsTable"); 
});
///////////////////////////////////////////////////////////////////////////////
/// html customizations  
///////////////////////////////////////////////////////////////////////////////
$('#logView ul').append('<button id="refreshLogButton"><img src="/_admin/html/media/icons/refresh_icon16.png" width=16 height=16></button><div id=tab_right align=right><form><input type="text" id="logSearchField"></input><button id="submitLogSearch">Search</button></form></div>');

///////////////////////////////////////////////////////////////////////////////
/// initialize jquery tabs 
///////////////////////////////////////////////////////////////////////////////

$("#tabs").tabs({
  select: function(event, ui) {
    if (ui.index == 0) {
      currentLoglevel = 5; 
      createLogTable(5); 
    }
    else {
      currentLoglevel = ui.index; 
      createLogTable(ui.index); 
    }
  }    
});


///////////////////////////////////////////////////////////////////////////////
/// checks for a login user cookie, creates new sessions if null  
///////////////////////////////////////////////////////////////////////////////

if ($.cookie("sid") == null) {
  $('#logoutButton').hide();
  $('#movetologinButton').show();
  $.ajax({
    type: "POST",
    url: "/_admin/user-manager/session/",
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      $.cookie("sid", data.sid);
      },
      error: function(data) {
      }
  });
}

///////////////////////////////////////////////////////////////////////////////
/// if user exists, then:   
///////////////////////////////////////////////////////////////////////////////

if ($.cookie("rights") != null || $.cookie("user") != null) {
  $('#loginWindow').hide();
  $('#movetologinButton').hide();
  $('#logoutButton').show();
  $('#activeUser').text($.cookie("user") + '!'); 
  currentUser = $.cookie("user"); 
}
else {
  $('#logoutButton').hide();
}

///////////////////////////////////////////////////////////////////////////////
/// draws collection table   
///////////////////////////////////////////////////////////////////////////////

var collectionTable = $('#collectionsTableID').dataTable({
    "bFilter": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": false, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{"sWidth":"150px", "bSortable":false}, null, null, null, null, null ],
    "oLanguage": {"sEmptyTable": "No collections"}
});

///////////////////////////////////////////////////////////////////////////////
/// draws the document edit table 
///////////////////////////////////////////////////////////////////////////////

var documentEditTable = $('#documentEditTableID').dataTable({
    "aaSorting": [],
    "bFilter": false,
    "bPaginate":false,
    "bSortable": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sClass":"read_only","bSortable": false, "sWidth": "30px"}, 
                  {"sClass":"writeable", "bSortable": false }, 
                  {"sClass":"writeable", "bSortable": false },
                  {"bVisible": false}], 
    "oLanguage": {"sEmptyTable": "No documents"}
});

///////////////////////////////////////////////////////////////////////////////
/// draws new doc view table 
///////////////////////////////////////////////////////////////////////////////

var newDocumentTable = $('#NewDocumentTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bSortable": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sClass":"read_only","bSortable": false, "sWidth": "170px"}, 
                  {"sClass":"writeable", "bSortable": false }, 
                  {"sClass":"writeable", "bSortable": false },
                  {"bVisible": false}] 
  });

///////////////////////////////////////////////////////////////////////////////
/// draws documents table  
///////////////////////////////////////////////////////////////////////////////

var documentsTable = $('#documentsTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bSortable": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sClass":"read_only","bSortable": false, "sWidth": "80px"}, 
                  {"sClass":"read_only","bSortable": false, "sWidth": "125px"}, 
                  {"sClass":"read_only","bSortable": false, "sWidth": "60px"}, 
                  {"bSortable": false}],
    "oLanguage": {"sEmptyTable": "No documents"}
  });

///////////////////////////////////////////////////////////////////////////////
/// draws edit collection table  
///////////////////////////////////////////////////////////////////////////////

var editCollectionTable = $('#editCollectionTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bSortable": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sClass":"read_only","bSortable": false, "sWidth": "100px"}, 
                  {"sClass":"read_only","bSortable": false}, 
                  {"sClass":"read_only","bSortable": false}] 
  });

///////////////////////////////////////////////////////////////////////////////
/// draws crit log table  
///////////////////////////////////////////////////////////////////////////////

var critLogTable = $('#critLogTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px"}, {"bSortable":false}], 
    "oLanguage": {"sEmptyTable": "No critical logfiles available"}
  });

///////////////////////////////////////////////////////////////////////////////
/// draws warn log table 
///////////////////////////////////////////////////////////////////////////////

var warnLogTable = $('#warnLogTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px"}, {"bSortable":false}],
    "oLanguage": {"sEmptyTable": "No warning logfiles available"}
  });
///////////////////////////////////////////////////////////////////////////////
/// draws info log table 
///////////////////////////////////////////////////////////////////////////////

var infoLogTable = $('#infoLogTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px"}, {"bSortable":false}],
    "oLanguage": {"sEmptyTable": "No info logfiles available"}
  });

///////////////////////////////////////////////////////////////////////////////
/// draws debug log table
///////////////////////////////////////////////////////////////////////////////

var debugLogTable = $('#debugLogTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px"}, {"bSortable":false}], 
    "oLanguage": {"sEmptyTable": "No debug logfiles available"}
  });

///////////////////////////////////////////////////////////////////////////////
/// draws "all" log table 
///////////////////////////////////////////////////////////////////////////////

var logTable = $('#logTableID').dataTable({
    "bFilter": false,
    "bPaginate":false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": true, 
    "iDisplayLength": -1,
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"center", "sWidth": "100px"}, {"bSortable":false}], 
    "oLanguage": {"sEmptyTable": "No logfiles available"}
  });

///////////////////////////////////////////////////////////////////////////////
/// creates layout using jquery ui   
///////////////////////////////////////////////////////////////////////////////

  $('body').layout({                                                                                               
    closable: false,             
    resizable: false,                                                                 
    applyDefaultStyles: false,   
    north__spacing_open:0,                                   
    north__spacing_closed: 0,                                                                           
    center__spacing_open:0,                                                      
    center__spacing_open:0, 
    south__spacing_closed: 0,  
    south__spacing_closed: 0 
  });

///////////////////////////////////////////////////////////////////////////////
/// location check    
///////////////////////////////////////////////////////////////////////////////

  $.address.change(function(event) {  

///////////////////////////////////////////////////////////////////////////////
/// Home   
///////////////////////////////////////////////////////////////////////////////

    if (location.hash == '' || location.hash =='#') {
      drawCollectionsTable();
      $('#subCenterView').hide();
      $('#centerView').show();
      $('#collectionsView').show(); 
      createnav("Collections"); 
    }

///////////////////////////////////////////////////////////////////////////////
/// new document table view (collection) 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 12) == "#collection?" ) {
      tableView = true; 
      var collectionID = location.hash.substr(12, location.hash.length); 
      var collID = collectionID.split("=");
      
      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collID[0],
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          collectionName = data.name;
          $('#nav2').text('-> ' + collectionName);
          $('#nav2').attr('href', '#showCollection?' +collID[0]);
        },
        error: function(data) {
        }
      });

      $('#nav1').text('Collections'); 
      $('#nav1').attr('href', '#');
      $('#nav3').text('-> New Document'); 

      newDocumentTable.fnClearTable(); 
      newDocumentTable.fnAddData(['<button id="deleteNewDocButton">delete</button>', "key", value2html("clicktoedit"), "clicktoedit" ]);
      documentTableMakeEditable('#NewDocumentTableID');
      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#newDocumentView').show();
      $('#newDocumentTableView').show();
      $('#newDocumentSourceView').hide();
    }

///////////////////////////////////////////////////////////////////////////////
///  showe edit documents view  
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 14) == "#editDocument?") {
      tableView = true; 
      var collectiondocID = location.hash.substr(14, location.hash.length); 
      collectionID = collectiondocID.split("/"); 

      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collectionID,
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          collectionName = data.name;
          $('#nav2').text('-> ' + collectionName);
          $('#nav2').attr('href', '#showCollection?' +collectionID[0]);
        },
        error: function(data) {
        }
      });
 
      $('#nav1').text('Collections');
      $('#nav1').attr('href', '#');
      $('#nav3').text('-> Edit:' + collectionID[1]); 

      $.ajax({
        type: "GET",
        url: "/document/" + collectiondocID,
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          $('#documentEditSourceBox').val(JSON.stringify(data));  
          documentEditTable.fnClearTable(); 
          hideAllSubDivs();
          $('#collectionsView').hide();
          $('#documentEditView').show();
          $('#documentEditSourceView').hide();
          $.each(data, function(key, val) {
            if (key == '_id' || key == '_rev') {
              documentEditTable.fnAddData(["", key, val, val]);
            }
            else {
                documentEditTable.fnAddData(['<button id="deleteEditedDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>',key, value2html(val), val]);
            }
          });
          documentTableMakeEditable('#documentEditTableID');
        },
        error: function(data) {
        }
      });
    }

///////////////////////////////////////////////////////////////////////////////
///  show colletions documents view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 16) == "#showCollection?") {
      var collectionID = location.hash.substr(16, location.hash.length); 
      
      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collectionID + "/count", 
        contentType: "application/json",
        processData: false,
        async: false,  
        success: function(data) {
          globalCollectionName = data.name;
          test = data.name; 
          collectionCount = data.count; 
          $('#nav2').text('-> ' + globalCollectionName);
        },
        error: function(data) {
        }
      });

      $('#nav1').text('Collections');
      $('#nav1').attr('href', '#');
      $('#nav2').attr('href', null);
      $('#nav3').text(''); 

      $.ajax({
        type: 'PUT',
        url: '/_api/simple/all/',
        data: '{"collection":"' + globalCollectionName + '","skip":0,"limit":10}', 
        contentType: "application/json",
        success: function(data) {
          $.each(data, function(k, v) {
            documentsTable.fnAddData(['<button id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, cutByResolution(JSON.stringify(v))]);  
          });
        },
        error: function(data) {
          
        }
      });
      documentsTable.fnClearTable(); 
      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#documentsView').show();
      totalCollectionCount = Math.ceil(collectionCount / 10); 
      collectionCurrentPage = 1;
      $('#documents_status').text("Showing page 1 of " + totalCollectionCount); 
    }

///////////////////////////////////////////////////////////////////////////////
///  shows edit collection view  
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 16)  == "#editCollection?") {
      var collectionID = location.hash.substr(16, location.hash.length); 
      var collectionName;    
 
      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collectionID,
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          collectionName = data.name;
          $('#nav2').text('-> Edit: ' + collectionName);
        },
        error: function(data) {
        }
      });

      $('#nav1').text('Collections');
      $('#nav1').attr('href', '#');

      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collectionID,
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          editCollectionTable.fnClearTable(); 
          $.each(data, function(key, val) {
            if (key == 'name') {
              editCollectionTable.fnAddData(["editable", key, val]); 
            }
            else if (key == 'status' || key == 'id') {
              editCollectionTable.fnAddData(["", key, val]);
            }
          });
          hideAllSubDivs();
          $('#collectionsView').hide();
          $('#editCollectionView').show();

          /* add custom cell class */ 
          var i = 0; 
          $('.read_only', editCollectionTable.fnGetNodes() ).each(function () {
            if ( i == 1) {
              $(this).removeClass('read_only');
              i = 0; 
            }
            if (this.innerHTML == "name") {
              i = 1; 
            }
          });

          $('#editCollectionTableID').dataTable().makeEditable({
            sUpdateURL: function(value, settings) {
              return value;        
            }
          });
        },
        error: function(data) {
          alert("error"); 
        }
      });
    }

///////////////////////////////////////////////////////////////////////////////
///  shows log view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#logs") {
      createLogTable(5); 
      hideAllSubDivs(); 
      $('#collectionsView').hide();
      $('#logView').show();
      createnav ("Logs"); 
    }

///////////////////////////////////////////////////////////////////////////////
///  shows status view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#status") {
      hideAllSubDivs(); 
      $('#collectionsView').hide();
      $('#statusView').show();
      createnav ("Status"); 
    }

///////////////////////////////////////////////////////////////////////////////
///  shows config view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#config") {
      hideAllSubDivs(); 
      $('#collectionsView').hide();
      $('#configView').show();
      createnav ("Config"); 
      var content={"Menue":{"Haha":"wert1", "ahha":"wert2"}, "Favoriten":{"top10":"content"},"Test":{"testing":"hallo 123 test"}}; 
      $("#configView").empty();

      $.each(content, function(data) {
        $('#configView').append('<h1>' + data + '</h1>');
        $.each(content[data], function(key, val) {
          $('#configView').append('<a>' + key + ":" + val + '</a><br>');
        });         
        //$('#configView').append('<a>' + menues + '</a><br>');
      }); 

    }

///////////////////////////////////////////////////////////////////////////////
///  shows query view  
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#query") {
      hideAllSubDivs(); 
      $('#collectionsView').hide();
      $('#queryView').show();
      createnav ("Query"); 
    }

///////////////////////////////////////////////////////////////////////////////
///  shows avocsh view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#avocsh") {
      hideAllSubDivs(); 
      $('#collectionsView').hide();
      $('#avocshView').show();
      createnav ("AvocSH"); 
    }

///////////////////////////////////////////////////////////////////////////////
///  shows create new collection view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#createCollection") {
      $('#nav1').attr('href', '#'); 
      $('#nav1').text('Collections');
      $('#nav2').text(': Create new collection');

      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#createCollectionView').show();

    }
  });

///////////////////////////////////////////////////////////////////////////////
/// saves edited document  
///////////////////////////////////////////////////////////////////////////////

  $('#saveEditedDocButton').live('click', function () {
    
    if (tableView == true) {
      var data = documentEditTable.fnGetData();
      var result = {}; 
      var collectionID; 

      for (row in data) {
        var row_data = data[row];
        if ( row_data[1] == "_id" ) {
          collectionID = row_data[3]; 
        } 
        else {
          //TODO
          result[row_data[1]] = JSON.parse(row_data[3]);
        }
        
      }

      $.ajax({
        type: "PUT",
        url: "/document/" + collectionID,
        data: JSON.stringify(result), 
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          tableView = true;
          var collID = collectionID.split("/");  
          window.location.href = "#showCollection?" + collID[0];  
          alert("done");
        },
        error: function(data) {
          alert(JSON.stringify(data)); 
        }
      });
    }
    else {
      var collectionID; 
      var boxContent = $('#documentEditSourceBox').val();
      var jsonContent = JSON.parse(boxContent);
      $.each(jsonContent, function(row1, row2) {
        if ( row1 == '_id') {
          collectionID = row2; 
        } 
      });

      $.ajax({
        type: "PUT",
        url: "/document/" + collectionID,
        data: JSON.stringify(jsonContent), 
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          tableView = true;
          var collID = collectionID.split("/");  
          window.location.href = "#showCollection?" + collID[0];  
          alert("done");
        },
        error: function(data) {
          alert(JSON.stringify(data)); 
        }
      });
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// live click function for refreshLogButton 
///////////////////////////////////////////////////////////////////////////////

  $('#refreshLogButton').live('click', function () {
    var selected = $("#tabs").tabs( "option", "selected" ); 
    switch (selected) {
    case 0:
      createLogTable(5);
      break; 
    default:
      createLogTable(selected);
      break;
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// adds a new row in edit document view 
///////////////////////////////////////////////////////////////////////////////

  $('#addEditedDocRowButton').live('click', function () {
    if (tableView == true) {
      documentEditTable.fnAddData(['<button id="deleteEditedDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>', "somevalue", value2html("editme"), 1337]);
      documentTableMakeEditable('#documentEditTableID');
    }
    else {
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// saves a new created document  
///////////////////////////////////////////////////////////////////////////////

  $('#saveNewDocButton').live('click', function () {
    
    if (tableView == true) {
      var data = newDocumentTable.fnGetData();
      var result = {}; 
      var collectionID = location.hash.substr(12, location.hash.length); 
      var collID = collectionID.split("="); 

      for (row in data) {
        var row_data = data[row];
        result[row_data[1]] = row_data[3];
      }

      $.ajax({
        type: "POST",
        url: "/document?collection=" + collID, 
        data: JSON.stringify(result), 
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          tableView = true;
          window.location.href = "#showCollection?" + collID;  
          alert("done");
        },
        error: function(data) {
          alert(JSON.stringify(data)); 
        }
      });
    }
    else {
      var collectionID = location.hash.substr(12, location.hash.length); 
      var collID = collectionID.split("="); 
      var boxContent = $('#NewDocumentSourceBox').val();
      var jsonContent = JSON.parse(boxContent);
      $.each(jsonContent, function(row1, row2) {
        if ( row1 == '_id') {
          collectionID = row2; 
        } 
      });

      $.ajax({
        type: "POST",
        url: "/document?collection=" + collID, 
        data: JSON.stringify(jsonContent), 
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          tableView = true;
          window.location.href = "#showCollection?" + collID;  
          alert("done");
        },
        error: function(data) {
          alert(JSON.stringify(data)); 
        }
      });
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// adds a new row in new document view  
///////////////////////////////////////////////////////////////////////////////

  $('#addNewDocButton').live('click', function () {
    if (tableView == true) {
      newDocumentTable.fnAddData(['<button id="deleteNewDocButton">delete</button>', "somevalue", value2html("editme"), "editme" ]);
      documentTableMakeEditable('#NewDocumentTableID');
    }
    else {
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// toggle button for source / table - edit document view 
///////////////////////////////////////////////////////////////////////////////

  $('#toggleEditedDocButton').live('click', function () {
    if (tableView == true) {
      try {
        var content = documentEditTable.fnGetData();
        var result = {}; 
 
        for (row in content) {
          var row_data = content[row];
          result[row_data[1]] = row_data[3];
        }
        $('#documentEditSourceBox').val(JSON.stringify(result));  
        $('#documentEditTableView').toggle();
        $('#documentEditSourceView').toggle();
        tableView = false; 
        var img = $(this).find('img'); 
        img.attr('src', '/_admin/html/media/icons/on_icon16.png');
      }
  
      catch(e) {
        alert(e); 
      }
    }
    else {
      try {
        var boxContent = $('#documentEditSourceBox').val();  
        parsedContent = JSON.parse(boxContent); 

        documentEditTable.fnClearTable(); 
        $.each(parsedContent, function(key, val) {
          if (key == '_id' || key == '_rev') {
            documentEditTable.fnAddData(["", key, val, val]);
          }
          else {
              documentEditTable.fnAddData(['<button id="deleteEditedDocButton">delete</button>',key, value2html(val), val]);
          }
        });
  
        documentTableMakeEditable ('#documentEditTableID'); 
        $('#documentEditTableView').toggle();
        $('#documentEditSourceView').toggle();
        tableView = true; 
        var img = $(this).find('img'); 
        img.attr('src', '/_admin/html/media/icons/off_icon16.png');
      }
 
      catch(e) {
        alert("Please make sure the entered value is a valid json string."); 
      }

    }
  });

///////////////////////////////////////////////////////////////////////////////
/// perform logout 
///////////////////////////////////////////////////////////////////////////////

  $('#logoutButton').live('click', function () {
      $.ajax({
        type: "PUT",
        url: '/_admin/user-manager/session/' + sid + '/logout',
        data: JSON.stringify({'user': currentUser}), 
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          $('#loginWindow').show();
          $('#logoutButton').hide();
          $('#movetologinButton').show();
          $('#activeUser').text('Guest!');  
          $.cookie("rights", null);
          $.cookie("user", null);
          currentUser = null;
          window.location.href = ""; 
        },
        error: function(data) {
        }
      });
  });

///////////////////////////////////////////////////////////////////////////////
/// perform login  
///////////////////////////////////////////////////////////////////////////////

  $('#loginButton').live('click', function () {
    var username = $('#usernameField').val();
    var password = $('#passwordField').val();
    var shapassword;

    if (password != '') {
      shapassword = $.sha256(password);   
    }
    else {
      shapassword = password;
    }

    $.ajax({
      type: "PUT",
      url: '/_admin/user-manager/session/' + sid + '/login',
      data: JSON.stringify({'user': username, 'password': shapassword}), 
      contentType: "application/json",
      processData: false, 
      success: function(data) {
        currentUser = $.cookie("user"); 
        $('#loginWindow').hide();
        $('#logoutButton').show();
        $('#movetologinButton').hide();
        $('#activeUser').text(username + '!');  
        $.cookie("rights", data.rights);
        $.cookie("user", data.user);

        /*animation*/
        $('#movetologinButton').text("Login");
	$('#footerSlideContent').animate({ height: '25px' });
	$('#footerSlideButton').css('backgroundPosition', 'top left');
	open = false;

        return false; 
      },
      error: function(data) {
        alert(JSON.stringify(data)); 
        return false; 
      }
    });
    return false; 
  });


///////////////////////////////////////////////////////////////////////////////
/// toggle button for source / table - new document view 
///////////////////////////////////////////////////////////////////////////////
  
  $('#toggleNewDocButton').live('click', function () {
    if (tableView == true) {
      try {
        var content = newDocumentTable.fnGetData();
        var result = {}; 
 
        for (row in content) {
          var row_data = content[row];
          result[row_data[1]] = row_data[3];
        }

        $('#NewDocumentSourceBox').val(JSON.stringify(result));  
        $('#NewDocumentTableView').toggle();
        $('#NewDocumentSourceView').toggle();
        tableView = false; 
        var img = $(this).find('img'); 
        img.attr('src', '/_admin/html/media/icons/on_icon16.png');
      }
  
      catch(e) {
        alert(e); 
      }
    }
    else {
      try {
        var boxContent = $('#NewDocumentSourceBox').val();  
        parsedContent = JSON.parse(boxContent); 

        newDocumentTable.fnClearTable(); 
        $.each(parsedContent, function(key, val) {
          if (key == '_id' || key == '_rev') {
            newDocumentTable.fnAddData(["", key, val, val]);
          }
          else {
              newDocumentTable.fnAddData(['<button id="deleteNewDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>',key, value2html(val), val]);
          }
        });
        documentTableMakeEditable ('#NewDocumentTableID'); 
        $('#NewDocumentTableView').toggle();
        $('#NewDocumentSourceView').toggle();
        tableView = true; 
        var img = $(this).find('img'); 
        img.attr('src', '/_admin/html/media/icons/off_icon16.png');
      }
 
      catch(e) {
        alert(e); 
      }

    }
  });

///////////////////////////////////////////////////////////////////////////////
/// deletes a row in edit document view 
///////////////////////////////////////////////////////////////////////////////

  $('#deleteEditedDocButton').live('click', function () {
    var row = $(this).closest("tr").get(0);
    documentEditTable.fnDeleteRow(documentEditTable.fnGetPosition(row));
  });

///////////////////////////////////////////////////////////////////////////////
/// submit log search 
///////////////////////////////////////////////////////////////////////////////

  $('#submitLogSearch').live('click', function () {  

    var content = $('#logSearchField').val();
    var selected = $("#tabs").tabs( "option", "selected" ); 
    
    var currentTableID; 
    if (selected == 1) {currentTableID = "#critLogTableID";  } 
    else if (selected == 2) {currentTableID = "#warnLogTableID";} 
    else if (selected == 3) {currentTableID = "#infoLogTableID";} 
    else if (selected == 4) {currentTableID = "#debugLogTableID";} 
 
    switch (selected) {
    case 0:
      if(content == '') {
        createLogTable(5); 
      }
      else {
        $('#logTableID').dataTable().fnClearTable();
        $('#logTableID_status').text('Showing all entries for: "' + content + '"'); 
        $.getJSON("/_admin/log?search=" + content + "&upto=5", function(data) {
          var totalAmount = data.totalAmount; 
          var items=[];
          var i=0; 
     
          $.each(data.lid, function () {
            $('#logTableID').dataTable().fnAddData([data.level[i], data.text[i]]);
            i++;
          });
        });
      }
      break; 
      default:
        if(content == '') {
          createLogTable(selected); 
        } 
        else { 
          $(currentTableID).dataTable().fnClearTable();
          $(currentTableID + '_status').text('Showing all entries for: "' + content + '"'); 
          $.getJSON("/_admin/log?search=" + content + "&level=" + selected, function(data) {
            var totalAmount = data.totalAmount; 
            var items=[];
            var i=0; 
            $.each(data.lid, function () {
              $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]);
              i++;
            });
          });
        }
    }
  return false;          
  });

///////////////////////////////////////////////////////////////////////////////
/// deletes a row in new document view
///////////////////////////////////////////////////////////////////////////////

  $('#deleteNewDocButton').live('click', function () {
    var row = $(this).closest("tr").get(0);
    newDocumentTable.fnDeleteRow(documentEditTable.fnGetPosition(row));
  });

///////////////////////////////////////////////////////////////////////////////
/// submit avocsh content 
///////////////////////////////////////////////////////////////////////////////
 
 $('#submitAvoc').live('click', function () {
    var data = $('#avocshContent').val();
    var client = "client:" + data;
 
    $('#avocshWindow').append('<p>' + client + '</p>');
  
    try {
      var server = "server:" + eval(data); 
      $('#avocshWindow').append('<p class="avocshSuccess">' + server + '</p>');
    }
    catch(e) {
      $('#avocshWindow').append('<p class="avocshError">Error:' + e + '</p>');
    }
    $("#avocshWindow").animate({scrollTop:$("#avocshWindow")[0].scrollHeight}, 1);
    $("#avocshContent").val('');
    return false; 
  });

///////////////////////////////////////////////////////////////////////////////
/// submit query content 
///////////////////////////////////////////////////////////////////////////////

  $('#submitQuery').live('click', function () {
      var data = {query:$('#queryContent').val()};
    $.ajax({
      type: "POST",
      url: "/_api/cursor",
      data: JSON.stringify(data), 
      contentType: "application/json",
      processData: false, 
      success: function(data) {
        $("#queryOutput").empty();
        $("#queryOutput").append('<b><font color=green>' + JSON.stringify(data) + '</font></b>'); 
      },
      error: function(data) {
        $("#queryOutput").empty();
        $("#queryOutput").append('<b><font color=red>' + JSON.stringify(data) + '</font></b>'); 
      }
    });
  });

///////////////////////////////////////////////////////////////////////////////
/// draws view of creating a new document
///////////////////////////////////////////////////////////////////////////////

  $('#addDocumentButton').live('click', function () {
    toSplit = window.location.hash;
    var collID = toSplit.split("?"); 
    window.location.href = "#collection?" + collID[1] + "=newDocument";  
  });

///////////////////////////////////////////////////////////////////////////////
/// delete a document
///////////////////////////////////////////////////////////////////////////////

  $('#documentsView tr td button').live('click', function () {
    var aPos = documentsTable.fnGetPosition(this.parentElement);
    var aData = documentsTable.fnGetData(aPos[0]);
    var row = $(this).closest("tr").get(0);
    var documentID = aData[1]; 
   
    if (this.id == "deleteDoc") { 
    try { 
      $.ajax({ 
        type: 'DELETE', 
        contentType: "application/json",
        url: "/document/" + documentID 
      });
    }

    catch(e) {
      alert(e); 
    }

    documentsTable.fnDeleteRow(documentsTable.fnGetPosition(row));
    }

    if (this.id == "editDoc") {
      window.location.href = "#editDocument?" + documentID; 
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// check top menue buttons 
///////////////////////////////////////////////////////////////////////////////

  $('#menue button').live('click', function () {
    if (this.id == "Collections") {
      $('#subCenterView').hide();
      $('#collectionsView').show();
      window.location.href = "#";
    }
    if (this.id == "Logs") {
      window.location.href = "#logs";
    }
    if (this.id == "Status") {
      window.location.href = "#status";
    }
    if (this.id == "Configuration") {
      window.location.href = "#config";
    }
    if (this.id == "Query") {
      window.location.href = "#query";
    }
    if (this.id == "AvocSH") {
      window.location.href = "#avocsh";
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// saves an edited collection 
///////////////////////////////////////////////////////////////////////////////

  $('#saveEditedCollection').live('click', function () {
    var newColName; 
    var currentid; 
    var data = editCollectionTable.fnGetData(); 
    
    $.each(data, function(info, key) {
      if (key[1] == 'name') {
        newColName = key[2]; 
      }
      if (key[1] == 'id') {
        currentid = key[2]; 
      }
    });
    
      $.ajax({
        type: "PUT",
        url: "/_api/collection/" + currentid + "/rename",
        data: '{"name":"' + newColName + '"}',  
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          alert("Collection renamed"); 
          window.location.href = ""; 
          $('#subCenterView').hide();
          $('#centerView').show();
          drawCollectionsTable(); 
        },
        error: function(data) {
          alert(JSON.stringify(data));  
        }
      });
  });

///////////////////////////////////////////////////////////////////////////////
/// cancels the creation of a new collection 
///////////////////////////////////////////////////////////////////////////////

  $('#cancelNewCollection').live('click', function () {
    window.location.href = "#"; 
  });

///////////////////////////////////////////////////////////////////////////////
/// saves a new collection  
///////////////////////////////////////////////////////////////////////////////

  $('#saveNewCollection').live('click', function () {
     
      var wfscheck = $('input:radio[name=waitForSync]:checked').val();
      var collName = $('#createCollName').val(); 
 
      $.ajax({
        type: "POST",
        url: "/_api/collection",
        data: '{"name":"' + collName + '", "waitForSync":"' + JSON.parse(wfscheck) + '"}',  
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          alert("Collection created"); 
          window.location.href = ""; 
          $('#subCenterView').hide();
          $('#centerView').show();
          drawCollectionsTable(); 
        },
        error: function(data) {
          alert(JSON.stringify(data));  
        }
      });
  }); 

///////////////////////////////////////////////////////////////////////////////
/// live-click function for buttons: "createCollection" & "refreshCollections" 
///////////////////////////////////////////////////////////////////////////////

  $('#centerView button').live('click', function () {
    if (this.id == "createCollection") {
      window.location.href = "#createCollection";
      return false; 
    }
    if (this.id == "refreshCollections") {
      drawCollectionsTable(); 
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// live-click function for buttons in "collectionsTableID td"  
///////////////////////////////////////////////////////////////////////////////

  $('#collectionsTableID td button').live('click', function () {
    var collectionsTable = $('#collectionsTableID').dataTable();
    var aPos = collectionsTable.fnGetPosition(this.parentElement);
    var aData = collectionsTable.fnGetData(aPos[0]);
    var collectionID = aData[1];
    var collectionName = aData[2]; 
     
///////////////////////////////////////////////////////////////////////////////
///delete a collection
///////////////////////////////////////////////////////////////////////////////

  if (this.id == "delete") {
    $( "#dialog-deleteCollection" ).dialog({
      resizable: false,
      height:180,
      modal: true,
      buttons: {
        "Delete collection": function() {
          $( this ).dialog( "close" );
            $.ajax({
              type: 'DELETE', 
              url: "/_api/collection/" + collectionID,
              success: function () {
                drawCollectionsTable();
              }, 
              error: function () {
                alert('Error'); 
              }
            });
          },
          Cancel: function() {
            $( this ).dialog( "close" );
        }
      }
    });
  }




///////////////////////////////////////////////////////////////////////////////
/// jump to edit collection view
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "edit") {
      window.location.href = "#editCollection?" + collectionID;
    }

///////////////////////////////////////////////////////////////////////////////
/// jump to doc view of a collection
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "showdocs" ) {
      window.location.href = "#showCollection?" + collectionID; 
    }

///////////////////////////////////////////////////////////////////////////////
/// load a collection into memory
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "load") {
      $.ajax({
        type: 'PUT', 
        url: "/_api/collection/" + collectionID + "/load",
        success: function () {
          alert('Collection: ' + collectionID + ' loaded');
          drawCollectionsTable();
        }, 
        error: function (data) {
          alert('Error:' + JSON.stringify(data));
          drawCollectionsTable();  
        }
      });
    }
 
///////////////////////////////////////////////////////////////////////////////
/// unload a collection from memory
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "unload") {
      $.ajax({
        type: 'PUT', 
        url: "/_api/collection/" + collectionID + "/unload",
        success: function () {
          alert('Collection: ' + collectionID + ' unloaded');
          drawCollectionsTable();
        }, 
        error: function () {
          alert('Error'); 
        }
      });
    }
  });
});

///////////////////////////////////////////////////////////////////////////////
/// hide all divs inside of #centerView
///////////////////////////////////////////////////////////////////////////////

function hideAllSubDivs () {
  $('#subCenterView').show();
  $('#collectionsEditView').hide();
  $('#newCollectionView').hide();
  $('#documentsView').hide();
  $('#documentEditView').hide(); 
  $('#newDocumentView').hide();
  $('#createCollectionView').hide();
  $('#editCollectionView').hide();  
  $('#logView').hide();
  $('#statusView').hide(); 
  $('#configView').hide();
  $('#avocshView').hide();
  $('#queryView').hide();  
}

///////////////////////////////////////////////////////////////////////////////
/// draw and fill collections table
///////////////////////////////////////////////////////////////////////////////

function drawCollectionsTable () {
  var collectionsTable = $('#collectionsTableID').dataTable();
  collectionsTable.fnClearTable();
  $.getJSON("/_api/collection/", function(data) {
    var items=[];
    $.each(data.collections, function(key, val) {
      var tempStatus = val.status;
      if (tempStatus == 1) {
        tempStatus = "new born collection";
      }
      else if (tempStatus == 2) {
        tempStatus = "unloaded";
        items.push(['<button id="delete"><img src="/_admin/html/media/icons/round_minus_icon16.png" width="16" height="16"></button><button id="load"><img src="/_admin/html/media/icons/connect_icon16.png" width="16" height="16"></button>', 
        val.id, val.name, tempStatus, "", ""]);
       }
      else if (tempStatus == 3) {
        tempStatus = "loaded";
        items.push(['<button id="delete"><img src="/_admin/html/media/icons/round_minus_icon16.png" width="16" height="16" title="Delete"></button><button id="unload"><img src="/_admin/html/media/icons/not_connected_icon16.png" width="16" height="16" title="Unload"></button><button id="showdocs"><img src="/_admin/html/media/icons/zoom_icon16.png" width="16" height="16" title="Show Documents"></button><button id="edit" title="Edit"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', 
        val.id, val.name, tempStatus, "", ""]);
      }
      else if (tempStatus == 4) {
        tempStatus = "in the process of being unloaded"; 
        items.push(["", val.id, val.name, tempStatus, "", ""]);
      }
      else if (tempStatus == 5) {
        tempStatus = "deleted"; 
        items.push(["", val.id, val.name, tempStatus, "", ""]);
      }
/*      else {
        tempStatus = "corrupted"; 
        items.push(["", "<font color=red>"+ val.id + "</font>", "<font color=red>"+ val.name + "</font>", "<font color=red>" + tempStatus + "</font>", "", ""]);
      }*/
    });
  $('#collectionsTableID').dataTable().fnAddData(items);
  showCursor();
  });
}

///////////////////////////////////////////////////////////////////////////////
/// short function to enable edit mode for a table 
///////////////////////////////////////////////////////////////////////////////

function documentTableMakeEditable (tableID) {
  var documentEditTable = $(tableID).dataTable();
  var i = 0; 
  $('.writeable', documentEditTable.fnGetNodes() ).each(function () {
    if ( i == 1) {
      $(this).removeClass('writeable');
      i = 0; 
    }
    if (this.innerHTML == "_id" || this.innerHTML == "_rev") {
      $(this).removeClass('writeable');
      i = 1; 
    }
  });
  //jeditable init
  $('.writeable', documentEditTable.fnGetNodes()).editable(function(value, settings) {
    var aPos = documentEditTable.fnGetPosition(this);
    if (aPos[1] == 1) {
      documentEditTable.fnUpdate(value, aPos[0], aPos[1]);
      return value;
    }
    if (aPos[1] == 2) {
      //TODO
      var oldContent = documentEditTable.fnGetData(aPos[0], aPos[1] + 1);
      console.log(oldContent);  
      var test = getTypedValue(value);
      if (String(value) == String(oldContent)) {
        // no change
        return value2html(oldContent);
      }
      else {
        // change
        documentEditTable.fnUpdate(test, aPos[0], aPos[1] + 1); 
        return value2html(test);
      } 
    } 
  },{
    data: function() {
      var aPos = documentEditTable.fnGetPosition(this);
      var value = documentEditTable.fnGetData(aPos[0], aPos[1]);

      if (aPos[1] == 1) {
        return value; 
      }
      if (aPos[1] == 2) {
        var oldContent = documentEditTable.fnGetData(aPos[0], aPos[1] + 1);
        if (typeof(oldContent) == 'object') {
          return JSON.stringify(oldContent); 
        }
        else {
          return oldContent;  
        }
      }
    }, 
    //type: 'textarea',
    tooltip: 'click to edit', 
    cssclass : 'jediTextarea', 
    submit: 'Okay',
    cancel: 'Cancel', 
    onblur: 'cancel',
    style: 'display: inline'
  });
}

///////////////////////////////////////////////////////////////////////////////
/// escape a string 
///////////////////////////////////////////////////////////////////////////////

function escaped (value) {
  return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#38;");
}

///////////////////////////////////////////////////////////////////////////////
/// get value of user edited tabel cell 
///////////////////////////////////////////////////////////////////////////////

function getTypedValue (value) {
  if (value == 'true') {
    return true;
  }
  if (value == 'false') {
    return false;
  }
  if (value == 'null') {
    return null;
  }
  if (value.match(/^-?((\d+)?\.)?\d+$/)) {
    // TODO: support exp notation
    return parseFloat(value);
  }
  
  try {
    // assume list or object
    var test = JSON.parse(value);
    if (test instanceof Array) {
      // value is an array
      return test;
    }
    if (typeof(test) == 'object') {
      // value is an object
      return test;
    }
  }
  catch (err) {
  }

  // fallback: value is a string
  value = value + '';
  
  if (value.substr(0, 1) == '"' && value.substr(-1) == '"' ) {
    value = value.substr(1, value.length-2);
  }
  return value;
}

///////////////////////////////////////////////////////////////////////////////
/// checks type fo typed cell value 
///////////////////////////////////////////////////////////////////////////////

function value2html (value) {
  var checked = typeof(value); 
  switch(checked) { 
    case 'number': 
    return ("number: " + value);
    case 'string': 
    return ("string: " + escaped(value));
    case 'boolean': 
    return ("boolean: " + value);
    case 'object':
    if (value instanceof Array) {
      return ("array: " + JSON.stringify(value));
    }
    else {
      return ("object: "+ JSON.stringify(value));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
/// draw breadcrumb navigation  
///////////////////////////////////////////////////////////////////////////////

function createnav (menue) {
  $('#nav1').text(menue);
  $('#nav2').text('');
  $('#nav3').text('');
  $('#nav4').text('');

  if (menue == "Collections") {
    $('#nav1').attr('href', '#');
  }
  else {
    $('#nav1').attr('href',null);
  } 
}

///////////////////////////////////////////////////////////////////////////////
/// animated footer div  
///////////////////////////////////////////////////////////////////////////////

$(function() {
  var open = false;  
  $('#footerSlideButton').click(function() {
    if(open === false) {
      $('#footerSlideContent').animate({ height: '120px' });
      $(this).css('backgroundPosition', 'bottom left');
      open = true;
      $('#movetologinButton').text("Hide");
    } 
    else {
      $('#footerSlideContent').animate({ height: '25px' });
      $(this).css('backgroundPosition', 'top left');
      open = false;
      $('#movetologinButton').text("Login");
    }
  });

  $('#movetologinButton').click(function() {
    if(open === false) {
      $('#movetologinButton').text("Hide");
      $('#footerSlideContent').animate({ height: '120px' });
      $('#footerSlideButton').css('backgroundPosition', 'bottom left');
      open = true;
    } 
    else {
      $('#movetologinButton').text("Login");
      $('#footerSlideContent').animate({ height: '25px' });
      $('#footerSlideButton').css('backgroundPosition', 'top left');
      open = false;
    }
  });
});

///////////////////////////////////////////////////////////////////////////////
/// Log tables pagination  
///////////////////////////////////////////////////////////////////////////////
function createLogTable(loglevel) { 
  currentPage = 1;  
  currentLoglevel = loglevel;  
  var url = "/_admin/log?level="+loglevel+"&size=10";

//set tableid  
  if (loglevel == 1) {currentTableID = "#critLogTableID";  } 
  else if (loglevel == 2) {currentTableID = "#warnLogTableID";} 
  else if (loglevel == 3) {currentTableID = "#infoLogTableID";} 
  else if (loglevel == 4) {currentTableID = "#debugLogTableID";} 
  else if (loglevel == 5) {
    currentTableID = "#logTableID";
    url = "/_admin/log?upto=4&size=10"; 
  } 
//get first rows
 
  $.getJSON(url, function(data) { 
    var items=[];
    var i=0; 
    currentAmount = data.totalAmount; 
    var totalPages = Math.ceil(currentAmount / 10); 
//clear table   
    $(currentTableID).dataTable().fnClearTable();
//draw first 10 rows
    $.each(data.lid, function () {
      $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]);
      i++;
    });

  if (totalPages == 0) {
    $(currentTableID + '_status').text("Showing page 0 of 0");
    return 0;  
  }

  $(currentTableID + '_status').text("Showing page " + currentPage + " of " + totalPages); 
  });
}

function createPrevDocPagination() {
  if (collectionCurrentPage == 1) {
    return 0; 
  }
  var prevPage = JSON.parse(collectionCurrentPage) -1; 
  var offset = prevPage * 10 - 10; 

  $('#documentsTableID').dataTable().fnClearTable();
  $.ajax({
    type: 'PUT',
    url: '/_api/simple/all/',
    data: '{"collection":"' + globalCollectionName + '","skip":' + offset + ',"limit":10}', 
    contentType: "application/json",
    success: function(data) {
      $.each(data, function(k, v) {
        $('#documentsTableID').dataTable().fnAddData(['<button id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, cutByResolution(JSON.stringify(v))]);  
      });
    },
    error: function(data) {        
    }
  });
  collectionCurrentPage = prevPage; 
  $('#documents_status').text("Showing page " + collectionCurrentPage + " of " + totalCollectionCount); 
}
function createNextDocPagination () {

  if (collectionCurrentPage == totalCollectionCount) {
    return 0; 
  }
   
  var nextPage = JSON.parse(collectionCurrentPage) +1; 
  var offset =  collectionCurrentPage * 10; 

  $('#documentsTableID').dataTable().fnClearTable();
  $.ajax({
    type: 'PUT',
    url: '/_api/simple/all/',
    data: '{"collection":"' + globalCollectionName + '","skip":' + offset + ',"limit":10}', 
    contentType: "application/json",
    success: function(data) {
      $.each(data, function(k, v) {
        $("#documentsTableID").dataTable().fnAddData(['<button id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, cutByResolution(JSON.stringify(v))]);  
      });
    },
    error: function(data) {        
    }
  });
  collectionCurrentPage = nextPage; 
  $('#documents_status').text("Showing page " + collectionCurrentPage + " of " + totalCollectionCount); 
}

function createPrevPagination(checked) {
  var prevPage = JSON.parse(currentPage) - 1; 
  var offset = prevPage * 10 - 10; 
  var url = "/_admin/log?level="+currentLoglevel+"&size=10&offset="+offset;
  var totalPages = Math.ceil(currentAmount / 10); 

  if (currentPage == 1 || totalPages == 0 ) {
    return 0; 
  }
  if (checked == "all") {
    url = "/_admin/log?upto=4&size=10&offset="+offset; 
  }

  $.getJSON(url, function(data) {
    $(currentTableID).dataTable().fnClearTable(); 

    var i = 0; 
    $.each(data.level, function() {
      $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]); 
      i++; 
    });
  currentPage = JSON.parse(currentPage) - 1; 
  $(currentTableID + '_status').text("Showing page " + currentPage + " of " + totalPages); 
  }); 
}

function createNextPagination(checked) { 

  var totalPages = Math.ceil(currentAmount / 10); 
 var offset = currentPage * 10; 
  var url = "/_admin/log?level="+currentLoglevel+"&size=10&offset="+offset;

  if (currentPage == totalPages || totalPages == 0 ) {
    return 0; 
  }
  if (checked == "all") {
    url = "/_admin/log?upto=4&size=10&offset="+offset; 
  }

  $.getJSON(url, function(data) {
    $(currentTableID).dataTable().fnClearTable();

    var i = 0; 
    $.each(data.level, function() {
      $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]); 
      i++
    });
    currentPage = JSON.parse(currentPage) + 1; 
    $(currentTableID + '_status').text("Showing page " + currentPage + " of " + totalPages); 
  });
}
    
function showCursor() {
  $(':button').mouseover(function () {
    $(this).css('cursor', 'pointer');
  });
}

function cutByResolution (string) {
  var userScreenSize = $(window).width();  
  var content; 
  var escapedContent = escaped(string); 
  var userContent; 

  if (userScreenSize <= 1024) {
    userContent = 150; 
  }
  else if (userScreenSize > 1024 && userScreenSize < 1680) {
    userContent = 250; 
  }
  else if (userScreenSize > 1680) {
    userContent = 350; 
  }

  if (escapedContent.length > userContent) { 
    content = escapedContent.substr(0,(userContent-3))+'...';   
  }
  else {
    content = escapedContent; 
  }
  return content; 
}

function createFirstPagination () {

  if (collectionCurrentPage == 1) {
    return 0; 
  }

  $('#documentsTableID').dataTable().fnClearTable();
  
  $.ajax({
    type: 'PUT',
    url: '/_api/simple/all/',
    data: '{"collection":"' + globalCollectionName + '","skip":0,"limit":10}', 
    contentType: "application/json",
    success: function(data) {
      $.each(data, function(k, v) {
        $('#documentsTableID').dataTable().fnAddData(['<button id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, cutByResolution(JSON.stringify(v))]);  
      });
      collectionCurrentPage = 1;
      $('#documents_status').text("Showing page 1 of " + totalCollectionCount); 
    },
    error: function(data) {
      
    }
  });
}

function createLastLogPagination (tableid) {
  var totalPages = Math.ceil(currentAmount / 10); 
  var offset = (totalPages * 10) - 10; 
  var url = "/_admin/log?level="+currentLoglevel+"&size=10&offset="+offset;

  if (currentPage == totalPages || totalPages == 0 ) {
    return 0; 
  }
  if (tableid = "#logTableID") {
    url = "/_admin/log?upto=4&size=10&offset="+offset; 
  }

  $.getJSON(url, function(data) {
    $(currentTableID).dataTable().fnClearTable();

    var i = 0; 
    $.each(data.level, function() {
      $(currentTableID).dataTable().fnAddData([data.level[i], data.text[i]]); 
      i++
    });
    currentPage = totalPages;  
    $(currentTableID + '_status').text("Showing page " + currentPage + " of " + totalPages); 
  });

}

function createLastPagination () {
  if (totalCollectionCount == collectionCurrentPage) {
    return 0
  }

  $('#documentsTableID').dataTable().fnClearTable();
  console.log(totalCollectionCount);
  
  var offset = totalCollectionCount * 10 - 10; 
  $.ajax({
    type: 'PUT',
    url: '/_api/simple/all/',
    data: '{"collection":"' + globalCollectionName + '","skip":' + offset + ',"limit":10}', 
    contentType: "application/json",
    success: function(data) {
      $.each(data, function(k, v) {
        $('#documentsTableID').dataTable().fnAddData(['<button id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._rev, cutByResolution(JSON.stringify(v))]);  
      });
      collectionCurrentPage = totalCollectionCount;
      $('#documents_status').text("Showing page " + totalCollectionCount + " of " + totalCollectionCount); 
    },
    error: function(data) {
    }
  });
}
