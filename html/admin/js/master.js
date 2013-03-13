///////////////////////////////////////////////////////////////////////////////
/// master.js 
/// arangodb js api  
///////////////////////////////////////////////////////////////////////////////
var welcomeMSG = "" 
+ "                                        _      \n"
+ "   __ _ _ __ __ _ _ __   __ _  ___  ___| |__   \n"
+ "  / _` | '__/ _` | '_ \\ / _` |/ _ \\/ __| '_ \\  \n"
+ " | (_| | | | (_| | | | | (_| | (_) \\__ \\ | | | \n"
+ "  \\__,_|_|  \\__,_|_| |_|\\__, |\\___/|___/_| |_| \n"
+ "                        |___/                  \n"
+ "                                               \n"
+ "Welcome to arangosh Copyright (c) triAGENS GmbH.";

var existingCharts; 
var statDivCount;  
// documents global vars
var collectionTotalPages;
var collectionCurrentPage;  
var globalDocumentCopy = { };
var globalCollectionName;  
var checkCollectionName; 
var printedHelp = false; 
var open = false;
var rowCounter = 0; 
var shArray = []; 
var BaseUrl = "/_admin/html/index.html"; 

try {
  if (start_color_print !== undefined) {
  }
}
catch (e1) {
  start_color_print = function () { };
}
try {
  if (stop_color_print !== undefined) {
  }
} 
catch (e2) {
  stop_color_print = function () { };
}
try {
  if (start_pretty_print !== undefined) {
  }
}
catch (e3) {
  start_pretty_print = function () { };
}
try {
  if (stop_pretty_print !== undefined) {
  }
}
catch (e4) {
  stop_pretty_print = function () { };
}

// a static cache
var CollectionTypes = { };

$(document).ready(function() {       
showCursor();
//hide incomplete functions 
$("#uploadFile").attr("disabled", "disabled");
$("#uploadFileImport").attr("disabled", "disabled");
$("#uploadFileSearch").attr("disabled", "disabled");

///////////////////////////////////////////////////////////////////////////////
/// global variables 
///////////////////////////////////////////////////////////////////////////////
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
  $(i + '_prev').live('click', function () {

    if ( i == "#logTableID" ) {
      createNextPagination("all");  
    }
    else {
      createNextPagination();  
    }
  });
  $(i + '_next').live('click', function () {
    if ( i == "#logTableID" ) {
      createPrevPagination("all");  
    }
    else {
      createPrevPagination();  
    }
  });
  $(i + '_last').live('click', function () {
    createLogTable(currentLoglevel);
  });
  $(i+ '_first').live('click', function () {
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
//statistics live click buttons close 
///////////////////////////////////////////////////////////////////////////////

$(".statsClose").live('click', function () {
  var divID = $(this).parent().parent();
  var chart = $(this).parent().parent().attr('id');
  var chartID = JSON.parse(chart.replace(/\D/g, '' ));
  var todelete; 

  $.each(existingCharts, function(x, i ) {
    var tempid = i.id; 
    if (tempid == chartID) {
      todelete = x; 
    } 
  });  
  
  existingCharts.splice(todelete, 1);  
  $("#chartBox"+chartID).remove();   

  stateSaving(); 
  
  if (temptop > 150 && templeft > 20) {
    templeft = templeft - 10; 
    temptop = temptop - 10;
  }

});

///////////////////////////////////////////////////////////////////////////////
// show statistic settings 
///////////////////////////////////////////////////////////////////////////////

$(".statsSettings").live('click', function () {
  var chartID = $(this).parent().next("div");
  var settingsID = $(this).parent().next("div").next("div");
  $(chartID).hide(); 
  $(settingsID).fadeIn(); 
});

///////////////////////////////////////////////////////////////////////////////
// show statistics charts
///////////////////////////////////////////////////////////////////////////////

$(".statsCharts").live('click', function () {
  var chartID = $(this).parent().next("div");
  var settingsID = $(this).parent().next("div").next("div");
  stateSaving(); 
  updateChartBoxes(); 
  $(settingsID).hide(); 
  $(chartID).fadeIn(); 
});

///////////////////////////////////////////////////////////////////////////////
/// html customizations  
///////////////////////////////////////////////////////////////////////////////
$('#logView ul').append('<button class="enabled" id="refreshLogButton"><img src="/_admin/html/media/icons/refresh_icon16.png" width=16 height=16></button><div id=tab_right align=right><form><input type="text" id="logSearchField" placeholder="Search..."></input><button id="submitLogSearch" style="visibility:hidden;"></button></form></div>');

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
/// disable grey'd out buttons
///////////////////////////////////////////////////////////////////////////////
  $(".nofunction").attr("disabled", "true");

///////////////////////////////////////////////////////////////////////////////
/// checks for a login user cookie, creates new sessions if null  
///////////////////////////////////////////////////////////////////////////////

// if ($.cookie("sid") == null) {
//   $('#logoutButton').hide();
//   $('#movetologinButton').show();
//   $.ajax({
//     type: "POST",
//     url: "/_admin/user-manager/session/",
//     contentType: "application/json",
//     processData: false, 
//     success: function(data) {
//       $.cookie("sid", data.sid);
//       },
//       error: function(data) {
//       }
//   });
// }

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
    "aaSorting": [[ 2, "desc" ]],
    "bPaginate": false, 
    "bFilter": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "bAutoWidth": false, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{"sWidth":"150px", "bSortable":false, "sClass":"leftCell"}, {"sWidth": "200px"}, {"sWidth": "200px"}, {"sWidth": "150px"}, null, {"sWidth": "100px"}, {"sWidth": "200px"}, {"sWidth": "200px", "sClass":"rightCell"} ],
    "aoColumnDefs": [{ "sClass": "alignRight", "aTargets": [ 6, 7 ] }],
    "oLanguage": {"sEmptyTable": "No collections"}
});

///////////////////////////////////////////////////////////////////////////////
/// draws the document edit table 
///////////////////////////////////////////////////////////////////////////////

var documentEditTable = $('#documentEditTableID').dataTable({
    "aaSorting": [[ 1, "desc" ]],
    "bAutoWidth": false, 
    "bFilter": false,
    "bPaginate":false,
    "bSortable": false,
    "bLengthChange": false, 
    "bDeferRender": true, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{"sClass":"read_only leftCell", "bSortable": false, "sWidth": "30px"}, 
                  {"sClass":"writeable", "bSortable": false, "sWidth":"400px" }, 
                  {"sClass":"writeable rightCell", "bSortable": false},
                  {"bVisible": false } ], 
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
    "aoColumns": [{ "sClass":"read_only center leftCell","bSortable": false, "sWidth": "30px"}, 
                  {"sClass":"writeable", "bSortable": false, "sWidth":"250px" }, 
                  {"sClass":"writeable rightCell", "bSortable": false },
                  {"bVisible": false } ] 
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
    "bAutoWidth": false, 
    "iDisplayLength": -1, 
    "bJQueryUI": true, 
    "aoColumns": [{ "sClass":"read_only leftCell", "bSortable": false, "sWidth":"80px"}, 
                 { "sClass":"read_only","bSortable": false, "sWidth": "200px"}, 
                 { "sClass":"read_only","bSortable": false, "sWidth": "100px"},  
                 { "sClass":"read_only","bSortable": false, "sWidth": "100px"},  
                 { "bSortable": false, "sClass": "cuttedContent rightCell"}],
    "oLanguage": { "sEmptyTable": "No documents"}
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
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}], 
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
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}],
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
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}],
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
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}], 
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
    "aoColumns": [{ "sClass":"center", "sWidth": "100px", "bSortable":false}, {"bSortable":false}], 
    "oLanguage": {"sEmptyTable": "No logfiles available"}
  });

///////////////////////////////////////////////////////////////////////////////
/// creates layout using jquery ui   
///////////////////////////////////////////////////////////////////////////////

  $('body').layout({                                                                                               
    closable: false,             
    resizable: false,                                                                 
    applyDefaultStyles: false, 
    north__spacing_open: 0,                                   
    north__spacing_closed: 0,                                                                           
    center__spacing_open: 0,                                                      
    south__spacing_open: 0, 
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
      highlightNav("#nav1");
      highlightNavButton("#Collections"); 
    }

///////////////////////////////////////////////////////////////////////////////
/// new document table view (collection) 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 12) == "#collection," ) {
      $("#addNewDocButton").removeAttr("disabled");
      tableView = true; 
      $('#toggleNewDocButtonText').text('Edit Source'); 
      
      var collectionID = location.hash.substr(12, location.hash.length); 
      var collID = collectionID.split(",");
      
      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collID[0] + "?" + getRandomToken(),
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          collectionName = data.name;
          $('#nav2').text(collectionName);
          $('#nav2').attr('href', '#showCollection,' +collID[0]);
          $('#nav1').attr('class', 'arrowbg');
        },
        error: function(data) {
        }
      });

      $('#nav1').text('Collections'); 
      $('#nav1').attr('href', '#');
      $('#nav2').attr('class', 'arrowbg');
      $('#nav3').text('new document'); 
      highlightNav("#nav3");

      newDocumentTable.fnClearTable(); 
      documentTableMakeEditable('#NewDocumentTableID');
      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#newDocumentView').show();
      $('#NewDocumentTableView').show();
      $('#NewDocumentSourceView').hide();
    }

///////////////////////////////////////////////////////////////////////////////
///  showe edit documents view  
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 14) == "#editDocument,") {
      tableView = true; 
      $("#addEditedDocRowButton").removeAttr("disabled");
      $('#toggleEditedDocButton').val('Edit Source'); 
      $('#toggleEditedDocButtonText').text('Edit Source'); 
      
      $('#documentEditSourceView').hide();
      $('#documentEditTableView').show();
      var collectiondocID = location.hash.substr(14, location.hash.length); 
      collectionID = collectiondocID.split("/"); 

      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collectionID + "?" + getRandomToken(),
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          collectionName = data.name;
          $('#nav2').text(collectionName);
          $('#nav2').attr('href', '#showCollection,' +collectionID[0]);
        },
        error: function(data) {
        }
      });
 
      $('#nav1').text('Collections');
      $('#nav1').attr('href', '#');
      $('#nav1').attr('class', 'arrowbg');
      $('#nav2').attr('class', 'arrowbg');
      $('#nav3').text('Edit document:' + collectionID[1]); 

      $.ajax({
        type: "GET",
        url: "/_api/document/" + collectiondocID + "?" + getRandomToken(),
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
            if (isSystemAttribute(key)) {
              documentEditTable.fnAddData(["", key, value2html(val, true), JSON.stringify(val)]);
            }
            else {
              documentEditTable.fnAddData(['<button class="enabled" id="deleteEditedDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>',key, value2html(val), JSON.stringify(val)]);
            }
          });
          documentTableMakeEditable('#documentEditTableID');
          showCursor();
        },
        error: function(data) {
        }
      });
    }

///////////////////////////////////////////////////////////////////////////////
///  show colletions documents view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 16) == "#showCollection,") {
      $('#nav1').removeClass('highlighted'); 
      var found = location.hash.match(/,([a-zA-Z0-9\-_]+)(,.+)?$/);
      if (! found) {
        throw "invalid URL";
      }
      globalAjaxCursorChange();

      $('#nav1').text('Collections');
      $('#nav1').attr('href', '#');
      $('#nav2').attr('href', null);
      $('#nav3').text(''); 
      highlightNav("#nav2");
      $("#nav3").removeClass("arrowbg");
      $("#nav2").removeClass("arrowbg");
      $("#nav1").addClass("arrowbg");

      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#documentsView').show();
      loadDocuments(found[1], collectionCurrentPage);
    }

///////////////////////////////////////////////////////////////////////////////
///  shows edit collection view  
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash.substr(0, 16)  == "#editCollection,") {
      var collectionID = location.hash.substr(16, location.hash.length); 
      var collectionName;
      var tmpStatus; 
 
      $.ajax({
        type: "GET",
        url: "/_api/collection/" + collectionID + "/properties" + "?" + getRandomToken(),
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          collectionName = data.name;
          $('#nav2').text('Edit: ' + collectionName);
          $('#editCollectionName').val(data.name); 
          $('#editCollectionID').text(data.id);
          $('#editCollectionType').text(collectionType(data));
          $('#editCollectionJournalSize').val(data.journalSize / 1024 / 1024);
          $('input:radio[name=editWaitForSync][value=' + String(data.waitForSync) + ']').attr("checked", "checked");

          switch (data.status) {
          case 1: tmpStatus = "new born collection"; break; 
          case 2: tmpStatus = "unloaded"; break; 
          case 3: tmpStatus = "loaded"; break; 
          case 4: tmpStatus = "in the process of being unloaded"; break; 
          case 5: tmpStatus = "deleted"; break; 
          }

          $('#editCollectionStatus').text(tmpStatus); 
          checkCollectionName = collectionName; 
        },
        error: function(data) {
        }
      });

      $('#nav1').text('Collections');
      $('#nav1').attr('href', '#');
      $('#nav1').attr('class', 'arrowbg');
      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#editCollectionView').show();
      $('#editCollectionName').focus();
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
      highlightNavButton("#Logs"); 
      showCursor();
    }

///////////////////////////////////////////////////////////////////////////////
///  shows status view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#status") {
      $('#graphBox').empty(); 
      stateReading(); 
      hideAllSubDivs(); 
      $('#collectionsView').hide();
      $('#statusView').show();
      createnav ("Statistics"); 
      highlightNavButton("#Status"); 
      makeDraggableAndResizable(); 
      //TODO
      createChartBoxes(); 
      updateGraphs(); 
    }

///////////////////////////////////////////////////////////////////////////////
///  shows query view  
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#query") {
      hideAllSubDivs(); 
      $('#collectionsView').hide();
      $('#queryView').show();
      createnav ("Query"); 
      highlightNavButton("#Query"); 
      $('#queryContent').focus();
    }

///////////////////////////////////////////////////////////////////////////////
///  shows avocsh view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#avocsh") {
      hideAllSubDivs(); 
      $('#avocshContent').val('');
      $('#collectionsView').hide();
      $('#avocshView').show();
      createnav ("ArangoDB Shell"); 
      highlightNavButton("#AvocSH"); 
      $('#avocshContent').focus();
      if (printedHelp === false) {
        print(welcomeMSG + require("org/arangodb/arangosh").HELP);
        printedHelp = true; 
        require("internal").startPrettyPrint(); 
      }
      $("#avocshContent").autocomplete({
        source: shArray
      });
    }

///////////////////////////////////////////////////////////////////////////////
///  shows create new collection view 
///////////////////////////////////////////////////////////////////////////////

    else if (location.hash == "#createCollection") {
      $('#nav1').attr('href', '#'); 
      $('#nav1').text('Collections');
      $('#nav2').text('Create new collection');
      $('#nav1').attr('class', 'arrowbg'); 
      highlightNavButton("#Collections"); 

      hideAllSubDivs();
      $('#collectionsView').hide();
      $('#createCollectionView').show();
      $('#createCollName').focus();
      $('#createCollName').val('');
      $('#createCollSize').val('');
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// saves edited document  
///////////////////////////////////////////////////////////////////////////////

  $('#saveEditedDocButton').live('click', function () {
    if (tableView == true) {
      var data = documentEditTable.fnGetData();
      var result = {}; 
      var documentID; 

      for (row in data) {
        var row_data = data[row];
        if ( row_data[1] == "_id" ) {
          documentID = JSON.parse(row_data[3]); 
        } 
        else {
          result[row_data[1]] = JSON.parse(row_data[3]);
        }
      }

      $.ajax({
        type: "PUT",
        url: "/_api/document/" + documentID,
        data: JSON.stringify(result), 
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          tableView = true;
          var collID = documentID.split("/");  
          window.location.href = "#showCollection," + collID[0];  
        },
        error: function(data) {
          alert(getErrorMessage(data));
        }
      });
    }
    else {
      try {
        var documentID = globalDocumentCopy._id;
        var boxContent = $('#documentEditSourceBox').val();
        boxContent = stateReplace(boxContent);
        parsedContent = JSON.parse(boxContent); 

        $.ajax({
          type: "PUT",
          url: "/_api/document/" + documentID,
          data: JSON.stringify(parsedContent), 
          contentType: "application/json",
          processData: false, 
          success: function(data) {
            tableView = true;
            $('#toggleEditedDocButton').val('Edit Source'); 
            var collID = documentID.split("/");  
            window.location.href = "#showCollection," + collID[0];  
          },
          error: function(data) {
           alert(getErrorMessage(data));
          }
        });
      }
      catch(e) {
        alert("Please make sure the entered value is a valid json string."); 
      }
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
      rowCounter = rowCounter + 1; 
      documentEditTable.fnAddData(['<button class="enabled" id="deleteEditedDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>', "somekey" + rowCounter, value2html("editme"), JSON.stringify("editme")]);
      documentTableMakeEditable('#documentEditTableID');
      showCursor();
    }
    else {
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// saves a new created document  
///////////////////////////////////////////////////////////////////////////////

  $('#saveNewDocButton').live('click', function () {
    var post;

    var found = location.hash.match(/,([a-zA-Z0-9\-_]+),.+$/);
    if (! found) {
      throw "invalid URL";
    }

    var collectionID = found[1];
    
    if (tableView == true) {
      var data = newDocumentTable.fnGetData();
      var result = {}; 

      for (row in data) {
        var row_data = data[row];
        result[row_data[1]] = JSON.parse(row_data[3]);
      }

      post = JSON.stringify(result);
    }
    else {
      var boxContent = $('#NewDocumentSourceBox').val();
      var jsonContent = JSON.parse(boxContent);
      $.each(jsonContent, function(row1, row2) {
        if ( row1 == '_id') {
          collectionID = row2; 
        } 
      });

      try {
        post = JSON.stringify(jsonContent);
      }
      catch(e) {
        alert("Please make sure the entered value is a valid json string."); 
      }
    }

    $.ajax({
      type: "POST",
      url: "/_api/" + collectionApiType(collectionID) + "?collection=" + collectionID, 
      data: post,
      contentType: "application/json",
      processData: false, 
      success: function(data) {
        tableView = true;
        window.location.href = "#showCollection," + collectionID;  
      },
      error: function(data) {
        alert(getErrorMessage(data));
      }
    });
  });

///////////////////////////////////////////////////////////////////////////////
/// adds a new row in new document view  
///////////////////////////////////////////////////////////////////////////////

  $('#addNewDocButton').live('click', function () {
    if (tableView == true) {
      rowCounter = rowCounter + 1; 
      newDocumentTable.fnAddData(['<button class="enabled" id="deleteNewDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>', "somekey" + rowCounter, value2html("editme"), JSON.stringify("editme")]);
      documentTableMakeEditable('#NewDocumentTableID');
      showCursor();
    }
    else {
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// toggle button for source / table - edit document view 
///////////////////////////////////////////////////////////////////////////////

  $('#toggleEditedDocButton').live('click', function () {
    if (tableView == true) {
        var content = documentEditTable.fnGetData();
        var result = {}; 
 
        for (row in content) {
          var row_data = content[row];
          result[row_data[1]] = JSON.parse(row_data[3]);
        }

        var copies = { }; // copy system attributes
        for (var a in systemAttributes()) {
          copies[a] = result[a];
          delete result[a];
        }
        globalDocumentCopy = copies;

        var myFormattedString = FormatJSON(result);
        $('#documentEditSourceBox').val(myFormattedString); 
        $('#documentEditTableView').toggle();
        $('#documentEditSourceView').toggle();
        tableView = false; 
        $('#toggleEditedDocButtonText').text('Edit Table'); 
        $("#addEditedDocRowButton").attr("disabled", "disabled");
    }
    else {
      try {
        var boxContent = $('#documentEditSourceBox').val(); 
        boxContent = stateReplace(boxContent);
        parsedContent = JSON.parse(boxContent); 
        documentEditTable.fnClearTable(); 
        $.each(parsedContent, function(key, val) {
          documentEditTable.fnAddData(['<button class="enabled" id="deleteEditedDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"</button>', key, value2html(val), JSON.stringify(val)]);
        });

        for (var a in systemAttributes()) {
          if (globalDocumentCopy[a] != undefined) {
            documentEditTable.fnAddData(['', a, value2html(globalDocumentCopy[a], true), JSON.stringify(globalDocumentCopy[a]) ]);
          }
        }
  
        documentTableMakeEditable ('#documentEditTableID'); 
        $('#documentEditTableView').toggle();
        $('#documentEditSourceView').toggle();
        tableView = true; 
        $('#toggleEditedDocButtonText').text('Edit Source'); 
        $("#addEditedDocRowButton").removeAttr("disabled");
      }
 
      catch(e) {
        alert("Please make sure the entered value is a valid json string."); 
      }

    }
  });

///////////////////////////////////////////////////////////////////////////////
/// cancel editing a doc  
///////////////////////////////////////////////////////////////////////////////

  $('#cancelEditedDocument').live('click', function () {
    var start = window.location.hash.indexOf(','); 
    var end = window.location.hash.indexOf('/'); 
    var collectionID = window.location.hash.substring(start + 1, end); 
    window.location.href = "#showCollection," + collectionID; 
    $('#nav1').removeClass('highlighted'); 
  });

///////////////////////////////////////////////////////////////////////////////
/// undo live changes of an edited document  
///////////////////////////////////////////////////////////////////////////////

  $('#undoEditedDocument').live('click', function () {
    location.reload();  
  });

///////////////////////////////////////////////////////////////////////////////
/// perform logout 
///////////////////////////////////////////////////////////////////////////////

//   $('#logoutButton').live('click', function () {
//       $.ajax({
//         type: "PUT",
//         url: '/_admin/user-manager/session/' + sid + '/logout',
//         data: JSON.stringify({'user': currentUser}), 
//         contentType: "application/json",
//         processData: false, 
//         success: function(data) {
//           $('#loginWindow').show();
//           $('#logoutButton').hide();
//           $('#movetologinButton').show();
//           $('#activeUser').text('Guest!');  
//           $.cookie("rights", null);
//           $.cookie("user", null);
//           currentUser = null;
//           window.location.href = ""; 
//         },
//         error: function(data) {
//         }
//       });
//   });

///////////////////////////////////////////////////////////////////////////////
/// perform login  
///////////////////////////////////////////////////////////////////////////////

//   $('#loginButton').live('click', function () {
//     var username = $('#usernameField').val();
//     var password = $('#passwordField').val();
//     var shapassword;

//     if (password != '') {
//       shapassword = $.sha256(password);   
//     }
//     else {
//       shapassword = password;
//     }

//     $.ajax({
//       type: "PUT",
//       url: '/_admin/user-manager/session/' + sid + '/login',
//       data: JSON.stringify({'user': username, 'password': shapassword}), 
//       contentType: "application/json",
//       processData: false, 
//       success: function(data) {
//         currentUser = $.cookie("user"); 
//         $('#loginWindow').hide();
//         $('#logoutButton').show();
//         $('#movetologinButton').hide();
//         $('#activeUser').text(username + '!');  
//         $.cookie("rights", data.rights);
//         $.cookie("user", data.user);

//         /*animation*/
//         $('#movetologinButton').text("Login");
//         $('#footerSlideContent').animate({ height: '25px' });
//         $('#footerSlideButton').css('backgroundPosition', 'top left');
//         open = false;

//         return false; 
//       },
//       error: function(data) {
//         alert(JSON.stringify(data)); 
//         return false; 
//       }
//     });
//     return false; 
//   });

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
          result[row_data[1]] = JSON.parse(row_data[3]);
        }

        var myFormattedString = FormatJSON(result)
        $('#NewDocumentSourceBox').val(myFormattedString); 
        $('#NewDocumentTableView').toggle();
        $('#NewDocumentSourceView').toggle();
        tableView = false; 
        $('#toggleNewDocButtonText').text('Edit Table'); 
        $("#addNewDocButton").attr("disabled", "disabled");
      }
  
      catch(e) {
        alert(e); 
      }
    }
    else {
      try {
        var boxContent = $('#NewDocumentSourceBox').val();  
        boxContent = stateReplace(boxContent);
        parsedContent = JSON.parse(boxContent); 

        newDocumentTable.fnClearTable(); 
        $.each(parsedContent, function(key, val) {
          if (isSystemAttribute(key)) {
            newDocumentTable.fnAddData(["", key, value2html(val, true), JSON.stringify(val)]);
          }
          else {
            newDocumentTable.fnAddData(['<button class="enabled" id="deleteNewDocButton"><img src="/_admin/html/media/icons/delete_icon16.png" width="16" height="16"></button>',key, value2html(val), JSON.stringify(val)]);
          }
        });
        documentTableMakeEditable ('#NewDocumentTableID'); 
        $('#NewDocumentTableView').toggle();
        $('#NewDocumentSourceView').toggle();
        tableView = true; 
        $('#toggleNewDocButtonText').text('Edit Source'); 
        $("#addNewDocButton").removeAttr("disabled");
      }
 
      catch(e) {
        alert(e); 
      }

    }
  });

///////////////////////////////////////////////////////////////////////////////
/// cancel new doc view 
///////////////////////////////////////////////////////////////////////////////

  $('#cancelNewDocument').live('click', function () {
    var start = window.location.hash.indexOf(','); 
    var end = window.location.hash.indexOf('=');  
    var collectionID = window.location.hash.substring(start + 1, end); 
    window.location.href = "#showCollection," + collectionID; 
    $('#nav1').removeClass('highlighted'); 
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

    hideLogPagination(); 
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
        $.getJSON("/_admin/log?search=" + content + "&upto=5&" + getRandomToken(), function(data) {
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
          $.getJSON("/_admin/log?search=" + content + "&level=" + selected + "&" + getRandomToken(), function(data) {
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

var lastFormatQuestion = true;  
 
 $('#submitAvoc').live('click', function () {
    var data = $('#avocshContent').val();

    var r = [ ];
    for (var i = 0; i < shArray.length; ++i) {
      if (shArray[i] != data) {
        r.push(shArray[i]);
      }
    }

    shArray = r;
    if (shArray.length > 4) {
      shArray.shift();
    }
    shArray.push(data);
   
    $("#avocshContent").autocomplete({
      source: shArray
    });

    if (data == "exit") {
      location.reload();
      return;  
    }

    var command;
    if (data == "help") {
      command = "require(\"org/arangodb/arangosh\").HELP";
    }
    else if (data == "reset") {
      command = "$('#avocshWindow').html(\"\");undefined;";
    }
    else {
      formatQuestion = JSON.parse($('input:radio[name=formatshellJSONyesno]:checked').val());
      if (formatQuestion != lastFormatQuestion) {
        if (formatQuestion == true) {
          require("internal").startPrettyPrint(); 
          lastFormatQuestion = true;
        } 
        if (formatQuestion == false) {
          require("internal").stopPrettyPrint(); 
          lastFormatQuestion = false;
        }
      }

      command = data;
    }

    var internal = require("internal");
    var client = "arangosh> " + internal.escapeHTML(data) + "<br>";
 
    $('#avocshWindow').append('<b class="avocshClient">' + client + '</b>');
    evaloutput(command);
    $("#avocshWindow").animate({scrollTop:$("#avocshWindow")[0].scrollHeight}, 1);
    $("#avocshContent").val('');
    return false; 
  });

  $('#refreshShell').live('click', function () {
    location.reload(); 
    return false; 
  });
///////////////////////////////////////////////////////////////////////////////
/// submit query content 
///////////////////////////////////////////////////////////////////////////////

  $('#submitQuery').live('click', function () {
    var data = {query:$('#queryContent').val()};
    var formattedJSON; 
    $.ajax({
      type: "POST",
      url: "/_api/cursor",
      data: JSON.stringify(data), 
      contentType: "application/json",
      processData: false, 
      success: function(data) {
        $("#queryOutput").empty();
        
        var formatQuestion = JSON.parse($('input:radio[name=formatJSONyesno]:checked').val());
        if (formatQuestion === true) {
          $("#queryOutput").append('<pre><font color=green>' + FormatJSON(data.result) + '</font></pre>'); 
        }
        else {
          $("#queryOutput").append('<a class="querySuccess"><font color=green>' + JSON.stringify(data.result) + '</font></a>'); 
        }
      },
      error: function(data) {
        var temp = JSON.parse(data.responseText);
        $("#queryOutput").empty();
        $("#queryOutput").append('<a class="queryError"><font color=red>[' + temp.errorNum + '] ' + temp.errorMessage + '</font></a>'); 
      }
    });
  });

///////////////////////////////////////////////////////////////////////////////
/// draws view of creating a new document
///////////////////////////////////////////////////////////////////////////////

  $('#addDocumentButton').live('click', function () {
    toSplit = window.location.hash;
    var collID = toSplit.split(",");
    window.location.href = "#collection," + collID[1] + ",newDocument";  
  });

///////////////////////////////////////////////////////////////////////////////
/// edit / delete a document
///////////////////////////////////////////////////////////////////////////////

  $('#documentsView tr td button').live('click', function () {
    var aPos = documentsTable.fnGetPosition(this.parentElement);
    var aData = documentsTable.fnGetData(aPos[0]);
    var documentID = aData[1];
   
    if (this.id == "deleteDoc") { 
      try { 
        $.ajax({ 
          type: 'DELETE', 
          contentType: "application/json",
          url: "/_api/document/" + documentID 
        });
      }
      catch(e) {
        alert(e); 
      }
      var row = $(this).closest("tr").get(0);
      documentsTable.fnDeleteRow(documentsTable.fnGetPosition(row));
      loadDocuments(globalCollectionName, collectionCurrentPage);
    }

    if (this.id == "editDoc") {
      window.location.href = "#editDocument," + documentID; 
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
      highlightNavButton ("#Collections");
    }
    else if (this.id == "Logs") {
      window.location.href = "#logs";
      highlightNavButton ("#Logs");
    }
    else if (this.id == "Status") {
      window.location.href = "#status";
      highlightNavButton ("#Status");
    }
    else if (this.id == "Configuration") {
      window.location.href = "#config";
      highlightNavButton ("#Configuration");
    }
    else if (this.id == "Documentation") {
      return 0; 
    }
    else if (this.id == "Query") {
      window.location.href = "#query";
      highlightNavButton ("#Query");
    }
    else if (this.id == "AvocSH") {
      window.location.href = "#avocsh";
      highlightNavButton ("#AvocSH");
    }
  });

///////////////////////////////////////////////////////////////////////////////
/// saves an edited collection 
///////////////////////////////////////////////////////////////////////////////

  $('#saveEditedCollection').live('click', function () {
    var newColName = $('#editCollectionName').val(); 
    var currentid = $('#editCollectionID').text();
    var journalSize = JSON.parse($('#editCollectionJournalSize').val() * 1024 * 1024);
    var wfscheck = $('input:radio[name=editWaitForSync]:checked').val();

    var wfs = (wfscheck == "true");
    var failed = false;

    if (newColName != checkCollectionName) {
      $.ajax({
        type: "PUT",
        async: false, // sequential calls!
        url: "/_api/collection/" + checkCollectionName + "/rename",
        data: '{"name":"' + newColName + '"}',  
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          alert("Collection renamed"); 
        },
        error: function(data) {
          alert(getErrorMessage(data));
          failed = true;
        }
      });
    }
   
    if (! failed) {
      $.ajax({
        type: "PUT",
        async: false, // sequential calls!
        url: "/_api/collection/" + newColName + "/properties",
        data: '{"waitForSync":' + (wfs ? "true" : "false") + ',"journalSize":' + JSON.stringify(journalSize) + '}',  
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          alert("Saved collection properties");
        },
        error: function(data) {
          alert(getErrorMessage(data));
          failed = true;
        }
      });
    }

    if (! failed) {
      window.location.href = BaseUrl;
      $('#subCenterView').hide();
      $('#centerView').show();
      drawCollectionsTable(); 
    }
    else {
      return 0;
    }

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
      var type = $('input:radio[name=type]:checked').val(); // collection type
      var collName = $('#createCollName').val(); 
      var collSize = $('#createCollSize').val();
      var journalSizeString;
      var isSystem = (collName.substr(0, 1) === '_');
      var wfs = (wfscheck == "true");

      if (collSize == '') { 
        journalSizeString = ''; 
      }
      else {
        collSize = JSON.parse(collSize) * 1024 * 1024;  
        journalSizeString = ', "journalSize":' + collSize; 
      }
      if (collName == '') {
        alert("No collection name entered. Aborting..."); 
        return 0; 
      } 

      $.ajax({
        type: "POST",
        url: "/_api/collection",
        data: '{"name":' + JSON.stringify(collName) + ',"waitForSync":' + JSON.stringify(wfs) + ',"isSystem":' + JSON.stringify(isSystem) + journalSizeString + ',"type":' + type + '}',
        contentType: "application/json",
        processData: false, 
        success: function(data) {
          alert("Collection created"); 
          window.location.href = BaseUrl;
          $('#subCenterView').hide();
          $('#centerView').show();
          drawCollectionsTable(); 
        },
        error: function(data) {
          alert(getErrorMessage(data));
        }
      });
  }); 

///////////////////////////////////////////////////////////////////////////////
/// live-click function for buttons: "createCollection" & "refreshCollections" 
///////////////////////////////////////////////////////////////////////////////

  $('#centerView button').live('click', function () {
    if (this.id == "createCollection") {
      window.location.href = "#createCollection";
      $('#footerSlideContainer').animate({ 'height': '25px' });
      $('#footerSlideContent').animate({ 'height': '25px' });
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
    var collectionID = aData[2];
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
      window.location.href = "#editCollection," + collectionID;
    }

///////////////////////////////////////////////////////////////////////////////
/// jump to doc view of a collection
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "showdocs" ) {
      window.location.href = "#showCollection," + collectionID; 
      $('#nav1').removeClass('highlighted'); 
    }

///////////////////////////////////////////////////////////////////////////////
/// load a collection into memory
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "load") {
      globalAjaxCursorChange();
      $.ajax({
        type: 'PUT', 
        url: "/_api/collection/" + collectionID + "/load",
        success: function () {
          drawCollectionsTable();
        }, 
        error: function (data) {
          var temp = JSON.parse(data.responseText);
          alert("Error: " + JSON.stringify(temp.errorMessage));  
          drawCollectionsTable();
        }
      });
    }
 
///////////////////////////////////////////////////////////////////////////////
/// unload a collection from memory
///////////////////////////////////////////////////////////////////////////////

    if (this.id == "unload") {
      globalAjaxCursorChange();
      $.ajax({
        type: 'PUT', 
        url: "/_api/collection/" + collectionID + "/unload",
        success: function () {
        }, 
        error: function () {
          alert('Error'); 
        }
      });
      drawCollectionsTable();
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
  $.getJSON("/_api/collection/?" + getRandomToken(), function(data) {
    var items=[];
    $.each(data.collections, function(key, val) {
      var tempStatus = val.status;
      if (tempStatus == 1) {
        tempStatus = "new born collection";
      }
      else if (tempStatus == 2) {
        tempStatus = "unloaded";
        items.push(['<button class="enabled" id="delete"><img src="/_admin/html/media/icons/round_minus_icon16.png" width="16" height="16"></button><button class="enabled" id="load"><img src="/_admin/html/media/icons/connect_icon16.png" width="16" height="16"></button><button><img src="/_admin/html/media/icons/zoom_icon16_nofunction.png" width="16" height="16" class="nofunction"></img></button><button><img src="/_admin/html/media/icons/doc_edit_icon16_nofunction.png" width="16" height="16" class="nofunction"></img></button>', 
        val.id, val.name, collectionType(val), tempStatus, "", "-", "-"]);
       }
      else if (tempStatus == 3) {
        tempStatus = "<font color=\"green\">loaded</font>";
        var alive; 
        var size; 
        var waitForSync;
      
        $.ajax({
          type: "GET",
          url: "/_api/collection/" + encodeURIComponent(val.name) + "/figures" + "?" + getRandomToken(),
          contentType: "application/json",
          processData: false,
          async: false,   
          success: function(data) {
            size = data.figures.journals.fileSize + data.figures.datafiles.fileSize;
            alive = data.figures.alive.count; 
            waitForSync = data.waitForSync;
          },
          error: function(data) {
          }
        });
        
        items.push(['<button class="enabled" id="delete"><img src="/_admin/html/media/icons/round_minus_icon16.png" width="16" height="16" title="Delete"></button><button class="enabled" id="unload"><img src="/_admin/html/media/icons/not_connected_icon16.png" width="16" height="16" title="Unload"></button><button class="enabled" id="showdocs"><img src="/_admin/html/media/icons/zoom_icon16.png" width="16" height="16" title="Show Documents"></button><button class="enabled" id="edit" title="Edit"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', 
        val.id, val.name, collectionType(val), tempStatus, waitForSync ? "yes" : "no", bytesToSize(size), alive]);
      }
      else if (tempStatus == 4) {
        tempStatus = "in the process of being unloaded"; 
        items.push(['<button id="delete"><img src="/_admin/html/media/icons/round_minus_icon16_nofunction.png" class="nofunction" width="16" height="16"></button><button class="enabled" id="load"><img src="/_admin/html/media/icons/connect_icon16.png" width="16" height="16"></button><button><img src="/_admin/html/media/icons/zoom_icon16_nofunction.png" width="16" height="16" class="nofunction"></img></button><button><img src="/_admin/html/media/icons/doc_edit_icon16_nofunction.png" width="16" height="16" class="nofunction"></img></button>', 
        val.id, val.name, collectionType(val), tempStatus, "", "-", "-"]);
      }
      else if (tempStatus == 5) {
        tempStatus = "deleted"; 
        items.push(["", val.id, val.name, collectionType(val), tempStatus, "", "-", "-"]);
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
    if (isSystemAttribute(this.innerHTML)) {
      $(this).removeClass('writeable');
      i = 1; 
    }
  });
  //jeditable init
  $('.writeable', documentEditTable.fnGetNodes()).editable(function(value, settings) {
    var aPos = documentEditTable.fnGetPosition(this);
    if (aPos[1] == 1) {
      //return key column
      documentEditTable.fnUpdate(value, aPos[0], aPos[1]);
      return value;
    }
    if (aPos[1] == 2) {
      var oldContent = JSON.parse(documentEditTable.fnGetData(aPos[0], aPos[1] + 1));
      var test = getTypedValue(value);
      if (String(value) == String(oldContent)) {
        // no change
        return value2html(oldContent);
      }
      else {
        // change update hidden row
       //MARKER 
        documentEditTable.fnUpdate(JSON.stringify(test), aPos[0], aPos[1] + 1); 
        // return visible row 
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
          //grep hidden row and paste in visible row
          return value2html(oldContent);
        }
        else {
          return oldContent;  
        }
      }
    }, 
    //type: 'textarea',
    type: "autogrow", 
    tooltip: 'click to edit', 
    cssclass : 'jediTextarea', 
    submit: 'Okay',
    cancel: 'Cancel', 
    onblur: 'cancel',
    //style: 'display: inline',
    autogrow: {lineHeight: 16, minHeight: 30}
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
  value = value.replace(/(^\s+|\s+$)/g, '');
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
  
  if (value !== '' && (value.substr(0, 1) != '"' || value.substr(-1) != '"')) {
    alert("You have entered an invalid string value. Please review and adjust it.");
    throw "error";
  }
   
  try {
    value = JSON.parse(value);
  }
  catch (e) {
    alert("You have entered an invalid string value. Please review and adjust it.");
    throw e;
  }
  return value;
}

///////////////////////////////////////////////////////////////////////////////
/// checks type fo typed cell value 
///////////////////////////////////////////////////////////////////////////////

function value2html (value, isReadOnly) {
  var typify = function (value) {
    var checked = typeof(value); 
    switch(checked) { 
      case 'number': 
        return ("<a class=\"sh_number\">" + value + "</a>");
      case 'string': 
        return ("<a class=\"sh_string\">"  + escaped(value) + "</a>");
      case 'boolean': 
        return ("<a class=\"sh_keyword\">" + value + "</a>");
      case 'object':
        if (value instanceof Array) {
          return ("<a class=\"sh_array\">" + escaped(JSON.stringify(value)) + "</a>");
        }
        else {
          return ("<a class=\"sh_object\">"+ escaped(JSON.stringify(value)) + "</a>");
        }
    }
  };
  return (isReadOnly ? "(read-only) " : "") + typify(value);
}

///////////////////////////////////////////////////////////////////////////////
/// draw breadcrumb navigation  
///////////////////////////////////////////////////////////////////////////////

function createnav (menue) {
  $('#nav1').text(menue);
  $('#nav1').attr('class', ''); 
  $('#nav2').text('');
  $('#nav2').attr('class', ''); 
  $('#nav3').text('');
  $('#nav3').attr('class', ''); 
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
  $('#footerSlideButton').click(function() {
    if(open === false) {
      $('#footerSlideContainer').animate({ 'height': '80px' });
      $('#footerSlideContent').animate({ 'height': '80px' });
      $(this).css('backgroundPosition', 'bottom left');
      open = true;
      $('#movetologinButton').text("Hide");
    } 
    else {
      $('#footerSlideContainer').animate({ 'height': '25px' });
      $('#footerSlideContent').animate({ 'height': '25px' });
      $(this).css('backgroundPosition', 'top left');
      open = false;
      $('#movetologinButton').text("Login");
    }
  });

  $('#movetologinButton').click(function() {
    if(open === false) {
      $('#movetologinButton').text("Hide");
      $('#footerSlideContent').animate({ height: '80px' });
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
 
  $("#logSearchField").val(""); 
  $(":input").focus(); 
  showLogPagination();
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
  $.getJSON(url + "&" + getRandomToken(), function(data) { 
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

function createFirstPagination () {
  if (collectionCurrentPage == 1) {
    return 0; 
  }

  loadDocuments(globalCollectionName, 1);
}

function createLastPagination () {
  if (collectionCurrentPage == collectionTotalPages) {
    return 0;
  }

  loadDocuments(globalCollectionName, collectionTotalPages);
}

function createPrevDocPagination() {
  if (collectionCurrentPage <= 1) {
    return 0; 
  }
  
  loadDocuments(globalCollectionName, collectionCurrentPage - 1);
}

function createNextDocPagination () {
  if (collectionCurrentPage >= collectionTotalPages) {
    return 0; 
  }
  
  loadDocuments(globalCollectionName, collectionCurrentPage + 1);
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

  $.getJSON(url + "&" + getRandomToken(), function(data) {
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

  $.getJSON(url + "&" + getRandomToken(), function(data) {
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
  $('.enabled').mouseover(function () {
    $(this).css('cursor', 'pointer');
  });
}

function cutByResolution (string) {
  if (string.length > 1024) {
    return escaped(string.substr(0, 1024)) + '...';
  }
  return escaped(string);
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

  $.getJSON(url + "&" + getRandomToken(), function(data) {
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

function globalAjaxCursorChange() {  
  $("html").bind("ajaxStart", function(){  
    $(this).addClass('busy');  
  }).bind("ajaxStop", function(){  
    $(this).removeClass('busy');  
  });  
} 

function FormatJSON(oData, sIndent) {
    if (arguments.length < 2) {
        var sIndent = "";
    }
    var sIndentStyle = "    ";
    var sDataType = RealTypeOf(oData);

    if (sDataType == "array") {
        if (oData.length == 0) {
            return "[]";
        }
        var sHTML = "[";
    } else {
        var iCount = 0;
        $.each(oData, function() {
            iCount++;
            return;
        });
        if (iCount == 0) { // object is empty
            return "{}";
        }
        var sHTML = "{";
    }

    // loop through items
    var iCount = 0;
    $.each(oData, function(sKey, vValue) {
        if (iCount > 0) {
            sHTML += ",";
        }
        if (sDataType == "array") {
            sHTML += ("\n" + sIndent + sIndentStyle);
        } else {
            sHTML += ("\n" + sIndent + sIndentStyle + JSON.stringify(sKey) + ": ");
        }

        // display relevant data type
        switch (RealTypeOf(vValue)) {
            case "array":
            case "object":
                sHTML += FormatJSON(vValue, (sIndent + sIndentStyle));
                break;
            case "boolean":
            case "number":
                sHTML += vValue.toString();
                break;
            case "null":
                sHTML += "null";
                break;
            case "string":
                sHTML += "\"" + vValue.replace(/\\/g, "\\\\").replace(/"/g, "\\\"") + "\"";
                break;
            default:
                sHTML += ("TYPEOF: " + typeof(vValue));
        }

        // loop
        iCount++;
    });

    // close object
    if (sDataType == "array") {
        sHTML += ("\n" + sIndent + "]");
    } else {
        sHTML += ("\n" + sIndent + "}");
    }

    // return
    return sHTML;
}

function RealTypeOf(v) {
  if (typeof(v) == "object") {
    if (v === null) return "null";
    if (v.constructor == (new Array).constructor) return "array";
    if (v.constructor == (new Date).constructor) return "date";
    if (v.constructor == (new RegExp).constructor) return "regex";
    return "object";
  }
  return typeof(v);
}

function bytesToSize(bytes, precision) {  
  var kilobyte = 1024;
  var megabyte = kilobyte * 1024;
  var gigabyte = megabyte * 1024;
  var terabyte = gigabyte * 1024;
   
  if ((bytes >= 0) && (bytes < kilobyte)) {
    return bytes + ' B';
   } 
   else if ((bytes >= kilobyte) && (bytes < megabyte)) {
     return (bytes / kilobyte).toFixed(precision) + ' KB';
   } 
   else if ((bytes >= megabyte) && (bytes < gigabyte)) {
     return (bytes / megabyte).toFixed(precision) + ' MB';
   } 
   else if ((bytes >= gigabyte) && (bytes < terabyte)) {
     return (bytes / gigabyte).toFixed(precision) + ' GB';
   } 
   else if (bytes >= terabyte) {
     return (bytes / terabyte).toFixed(precision) + ' TB';
   }   
   else {
     return bytes + ' B';
   }
}

function stateReplace (value) {
  var inString = false;
  var length = value.length;
  var position = 0;
  var escaped = false;

  var output = "";
  while (position < length) {
    var c = value.charAt(position++);

    if (c === '\\') {
      if (escaped) {
        /* case: \ followed by \ */
        output += '\\\\';
        escaped = false;
      } 
      else {
        /* case: single backslash */
        escaped = true;
      }
    }
    else if (c === '"') {
      if (escaped) {
        /* case: \ followed by " */
        output += '\\"';
        escaped = false;
      } 
      else {
        output += '"';
        inString = !inString;
      }
    } 
    else {
      if (inString) {
        if (escaped) {
          output += '\\' + c;
          escaped = false;
        }
        else {
          switch (c) {
            case '\b':
              output += '\\b';
              break;
            case '\f':
              output += '\\f';
              break;
            case '\n':
              output += '\\n';
              break;
            case '\t':
              output += '\\t';
              break;
            case '\r':
              output += '\\r';
              break;
            default:
              output += c;
              break;
          }
        }
      } 
      else {
        if (c >= '!') {
          output += c;
        }
      }
    }
  }

  return output;
}

$('#submitDocPageInput').live('click', function () {
  loadDocuments(globalCollectionName, parseInt($('#docPageInput').val()));
  return false; 
});



function highlightNav (string) {
  $("#nav1").removeClass("nonhighlighted");
  $("#nav2").removeClass("nonhighlighted");
  $("#nav3").removeClass("nonhighlighted");
  $("#nav4").removeClass("nonhighlighted");
  $(string).addClass("highlighted");
}

function makeStringEditable () {
  $('.editString').editable(function(value, settings) { 
    return(value);
  },{
    tooltip   : 'Click to edit string...',
    width     : '200px',
    height    : '20px'
  });
}

function makeIntEditable () {
  $('.editInt').editable(function(value, settings) {
    if (is_int(value) == true) {
      return value; 
    }
    else {
      alert("Only integers allowed!"); 
      return 0;
    } 
  },{
    tooltip   : 'Click to edit integer...',
    width     : '200px',
    height    : '20px'
  });
}

function is_int(value){
  if((parseFloat(value) == parseInt(value)) && !isNaN(value)){
      return true;
  } else {
      return false;
  }
}

function showLogPagination () {
  $('#logToolbarAll').show(); 
  $('#logToolbarCrit').show(); 
  $('#logToolbarWarn').show(); 
  $('#logToolbarDebu').show(); 
  $('#logToolbarInfo').show(); 
}

function hideLogPagination() {
  $('#logToolbarAll').hide(); 
  $('#logToolbarCrit').hide(); 
  $('#logToolbarWarn').hide(); 
  $('#logToolbarDebu').hide(); 
  $('#logToolbarInfo').hide(); 
}

function evaloutput (data) {
  try {
    var result = eval(data); 

    if (result !== undefined) {
      print(result);
    }
  }
  catch(e) {
    $('#avocshWindow').append('<p class="avocshError">Error:' + e + '</p>');
  }
}

function validate(evt) {
  var theEvent = evt || window.event;
  var key = theEvent.keyCode || theEvent.which;

  if ((key == 37 || key == 39) && !theEvent.shiftKey) {
    return;
  }
  key = String.fromCharCode( key );
  var regex = /[0-9\.\b]/;
  if( !regex.test(key) ) {
    theEvent.returnValue = false;
    if(theEvent.preventDefault) theEvent.preventDefault();
  }
}

///////////////////////////////////////////////////////////////////////////////
/// draw hours statistics 
///////////////////////////////////////////////////////////////////////////////

function drawConnections (placeholder, granularity) {
  var array = [];
  $.ajax({
    type: "GET",
    url: "/_admin/connection-statistics?granularity=" + granularity + "&" + getRandomToken(),
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      if (data.start.length == data.httpDuration.count.length) {
        var counter = 0; 
        for (i=0; i < data.start.length; i++) {
          array.push([data.start[i]*1000,data.httpDuration.count[i]]);
        }

        var options = {
          legend: {
            margin: 0,  
            show: true,
            backgroundOpacity: 0.4 
          }, 
          series: {
            lines: { show: true, 
                     steps: false, 
                     fill: true, 
                     lineWidth: 0.5, 
                     fillColor: { colors: [ { opacity: 0.6 }, { opacity: 0.6 } ] } 
            },
            points: { show: false },
            label: "Connections"
          }, 
          xaxis: {
            mode: "time",
            timeformat: "%h:%M", 
            twelveHourClock: false
          },
          //crosshair: { mode: "x" },
          grid: { hoverable: true, autoHighlight: false },
          colors: ["#9EAF5A"],
          grid: {
            backgroundColor: { colors: ["#fff", "#eee"] },
            borderWidth: 1,
            hoverable: true 
          }
        };

        $.plot($(placeholder), [array], options);

        $(placeholder).qtip({
          prerender: true,
          content: 'Loading...', // Use a loading message primarily
          position: {
            viewport: $(window), // Keep it visible within the window if possible
            target: 'mouse', // Position it in relation to the mouse
            adjust: { y: -30 } // ...but adjust it a bit so it doesn't overlap it.
          },
          show: false, // We'll show it programatically, so no show event is needed
          style: {
            classes: 'ui-tooltip-tipsy',
            tip: false 
          }
        });

        $(placeholder).bind('plothover', function(event, coords, item) {
          var self = $(this),
          api = $(this).qtip(), previousPoint, content; 


          if(!item) {
            api.cache.point = false;
            return api.hide(event);
          }

          previousPoint = api.cache.point;
          if(previousPoint !== item.dataIndex)  {
            api.cache.point = item.dataIndex;
            // Setup new content
            var date = new Date(item.datapoint[0]); 
            var hours = date.getHours(); 
            var minutes = date.getMinutes(); 
            var formattedTime = hours + ':' + minutes; 
            content = item.series.label + '(' + formattedTime + ') = ' + item.datapoint[1];
            // Update the tooltip content
            api.set('content.text', content);
            // Make sure we don't get problems with animations
            api.elements.tooltip.stop(1, 1);
            // Show the tooltip, passing the coordinates
            api.show(coords);
          }
        });

      }
    },
    error: function(data) {
    }
  });
}

///////////////////////////////////////////////////////////////////////////////
/// draw requests (granularity) 
///////////////////////////////////////////////////////////////////////////////

function drawRequests (placeholder, granularity) {
  var arraySent = [];
  var arrayReceived = [];
  var boxCount = placeholder.replace(/\D/g, '' );  
 
  $.ajax({
    type: "GET",
    url: "/_admin/request-statistics?granularity=" + granularity + "&" + getRandomToken(),
    contentType: "application/json",
    processData: false, 
    success: function(data) {
 
      if (data.start.length == data.bytesSent.count.length && data.start.length == data.bytesReceived.count.length) {
        var counter = 0; 
        for (i=0; i < data.start.length; i++) {
          arraySent.push([data.start[i]*1000,data.bytesSent.mean[i]]);
          arrayReceived.push([data.start[i]*1000,data.bytesReceived.mean[i]]);
        }
        var stack = 0, bars = false, lines = false, steps = false;
        var options = {
          legend: {
            show: true,
            margin: 0,  
            noColumns: 0, 
            backgroundOpacity: 0.4 
          }, 
          series: {
            lines: { show: true, 
                     steps: false,  
                     fill: true, 
                     lineWidth: 0.5,
                     fillColor: { colors: [ { opacity: 0.6 }, { opacity: 0.7 } ] } 
            }
          }, 
          xaxis: {
            mode: "time",
            timeformat: "%h:%M", 
            twelveHourClock: false
          },
          //crosshair: { mode: "x" },
          colors: ["#9EAF5A", "#5C3317"],
          grid: {
            mouseActiveRadius: 5, 
            backgroundColor: { colors: ["#fff", "#eee"] },
            borderWidth: 1,
            hoverable: true 
          }
        };

        var sent = $('input:checkbox[name=bytessent'+boxCount+']:checked').val(); 
        var received = $('input:checkbox[name=bytesreceived'+boxCount+']:checked').val(); 

        if ( sent == "on" && received == "on" ) {
          $.plot($(placeholder), [{ label: "Bytes sent", data: arraySent}, {label: "Bytes received", data:  arrayReceived}], options);
        }
        else if ( sent == "on" && received == undefined ) {
          $.plot($(placeholder), [{ label: "Bytes sent", data: arraySent}], options);
        }   
        else if ( sent == undefined && received == "on" ) {
          $.plot($(placeholder), [{ label: "Bytes received", data: arrayReceived}], options);
        }
        else if ( sent == undefined && received == undefined ) {
          $.plot($(placeholder), [], options);
        }

        $(placeholder).qtip({
          prerender: true,
          content: 'Loading...', // Use a loading message primarily
          position: {
            viewport: $(window), // Keep it visible within the window if possible
            target: 'mouse', // Position it in relation to the mouse
            adjust: { y: -30 } // ...but adjust it a bit so it doesn't overlap it.
          },
          show: false, // We'll show it programatically, so no show event is needed
          style: {
            classes: 'ui-tooltip-tipsy',
            tip: false 
          }
        });

        $(placeholder).bind('plothover', function(event, coords, item) {
          var self = $(this),
          api = $(this).qtip(), previousPoint, content; 

          if(!item) {
            api.cache.point = false;
            return api.hide(event);
          }

          previousPoint = api.cache.point;
          if(previousPoint !== item.dataIndex)  {
            api.cache.point = item.dataIndex;
            // Setup new content
            var date = new Date(item.datapoint[0]); 
            var hours = date.getHours(); 
            var minutes = date.getMinutes(); 
            var formattedTime = hours + ':' + minutes; 
            if (item.series.label == "Bytes received") {
              content = item.series.label + '(' + formattedTime + ') = ' + item.datapoint[2];
            }
            else {
              content = item.series.label + '(' + formattedTime + ') = ' + item.datapoint[1];
            } 
            // Update the tooltip content
            api.set('content.text', content);
            // Make sure we don't get problems with animations
            api.elements.tooltip.stop(1, 1);
            // Show the tooltip, passing the coordinates
            api.show(coords);
          }
        });

      }
    },
    error: function(data) {
    }
  });
}

function updateGraphs() {
  if (location.hash == "#status") {
    updateChartBoxes(); 
    setTimeout(updateGraphs, 60000);
  }
  else {
  }
}

$(document).delegate('#btnAddNewStat', 'mouseleave', function () { setTimeout(function(){ if (!ItemActionButtons.isHoverMenu) { $('#btnSaveExtraOptions').hide(); }}, 100, 1) });
$(document).delegate('#btnSaveExtraOptions', 'mouseenter', function () { ItemActionButtons.isHoverMenu = true; });
$(document).delegate('#btnSaveExtraOptions', 'mouseleave', function () { $('#btnSaveExtraOptions').hide(); ItemActionButtons.isHoverMenu = false; });
 
var $IsHoverExtraOptionsFlag = 0;
 
$(document).ready(function () {
  $(".button").button();
  $(".buttonset").buttonset();
 
  $('#btnAddNewStat').button({ icons: { primary: "ui-icon-plusthick" } });
  $('#btnSaveExtraOptions li').addClass('ui-corner-all ui-widget');
  $('#btnSaveExtraOptions li').hover(
    function () {
      $(this).addClass('ui-state-default'); 
    },
    function () {  
      $(this).removeClass('ui-state-default'); 
    }
  );
  $('#btnSaveExtraOptions li').mousedown(function () { 
    $(this).addClass('ui-state-active'); 
  });
  $('#btnSaveExtraOptions li').mouseup(function () { 
    $(this).removeClass('ui-state-active'); 
  });
});
 
var ItemActionButtons = {

  isHoverMenu: false,
 
  AllowDelete: function (value) { value ? $("#btnDelete").show() : $("#btnDelete").hide() },
  AllowCancel: function (value) { value ? $("#btnCancel").show() : $("#btnCancel").hide() },
  AllowSave: function (value) { value ? $("#btnSave").show() : $("#btnSave").hide() },
  AllowSaveExtra: function (value) { value ? $("#btnAddNewStat").show() : $("#btnAddNewStat").hide() },
 
  onDeleteClick: function () { },
  onCancelClick: function () { },
  onSaveClick: function () { },
  onSaveExtraClick: function () {
    $('#btnSaveExtraOptions').toggle();
    var btnLeft = $('#divSaveButton').offset().left;
    var btnTop = $('#divSaveButton').offset().top + $('#divSaveButton').outerHeight(); // +$('#divSaveButton').css('padding');
    var btnWidth = $('#divSaveButton').outerWidth();
    $('#btnSaveExtraOptions').css('left', btnLeft).css('top', btnTop);
  },
  ShowConnectionsStats: function () {
    var chartcount = JSON.parse(statDivCount)+1; 
    existingCharts.push({type:"connection", granularity:"minutes", top:50, left:50, id:JSON.parse(chartcount)});
    statDivCount = chartcount; 
    createSingleBox (chartcount, "connection"); 
  },
  ShowRequestsStats: function () { 
    var chartcount = JSON.parse(statDivCount)+1; 
    existingCharts.push({type:"request", granularity:"minutes", sent:true, received:true, top:50, left: 50, id:chartcount});
    statDivCount = chartcount;
    createSingleBox (chartcount, "request", true);
  }
}

$.fn.isVisible = function() {
  return $.expr.filters.visible(this[0]);
};

function stateSaving () {
  $.each(existingCharts, function(v, i ) {
    // position statesaving 
    var tempcss = $("#chartBox"+this.id).position();  
    // size statesaving 
    var tempheight = $("#chartBox"+this.id).height();
    var tempwidth = $("#chartBox"+this.id).width();
    // granularity statesaving 
    var grantouse = $('input[name=boxGranularity'+this.id+']:checked').val();

    if (i.type == "request") {
      var sent = $('input:checkbox[name=bytessent'+this.id+']:checked').val(); 
      var received = $('input:checkbox[name=bytesreceived'+this.id+']:checked').val(); 
      if ( sent == "on" ) {
        this.sent = true; 
      } 
      else {
        this.sent = false; 
      }
      if ( received = "on" ) {
        this.received = true;  
      }
      else {
        this.received = false; 
      }
    }
 
    // edit obj
    this.top = tempcss.top;
    this.left = tempcss.left;
    this.width = tempwidth; 
    this.height = tempheight; 
    this.granularity = grantouse;  
  });
  //write obj into local storage
  localStorage.setItem("statobj", JSON.stringify(existingCharts)); 
  localStorage.setItem("statDivCount", statDivCount); 
}

function stateReading () {

  try { 
    existingCharts = JSON.parse(localStorage.getItem("statobj"));  
    statDivCount = localStorage.getItem("statDivCount");  
    if (existingCharts != null) {
    }
    else {
      existingCharts = [];
      statDivCount = 0;  
    }
  }

  catch (e) {
    alert("no data:" +e); 
  }
}
 
function createChartBoxes () {
  $.each(existingCharts, function(v, i ) {
    var boxCount = i.id; 
    $("#graphBox").append('<div id="chartBox'+boxCount+'" class="chartContainer resizable draggable"/>');
    $("#chartBox"+boxCount).css({top: i.top, left: i.left}); 
    $("#chartBox"+boxCount).css({width: i.width, height: i.height}); 
    $("#chartBox"+boxCount).append('<div class="greydiv"/>');
    $("#chartBox"+boxCount).append('<div class="placeholderBox" id="placeholderBox'+boxCount+'"/>');
    $("#chartBox"+boxCount).append('<div class="placeholderBoxSettings" id="placeholderBoxSettings'+boxCount+'" style="display:none"/>');
    $("#chartBox"+boxCount+" .greydiv").append('<button class="statsClose"><img width="16" height="16" src="/_admin/html/media/icons/whitedelete_icon16.png"></button>');
    $("#chartBox"+boxCount+" .greydiv").append('<button class="statsSettings"><img width="16" height="16" src="/_admin/html/media/icons/settings_icon16.png"></button>');
    $("#chartBox"+boxCount+" .greydiv").append('<button class="statsCharts"><img width="16" height="16" src="/_admin/html/media/icons/whitechart_icon16.png"></button>');

    $("#chartBox"+boxCount+" .placeholderBoxSettings").append('<form action="#" id="graphForm'+boxCount+'" class="radioFormat">' +
      '<input type="radio" id="chartBox'+boxCount+'minutes" name="boxGranularity'+boxCount+'" value="minutes" checked/>' +
      '<label for="chartBox'+boxCount+'minutes" class="cb-enable selected">Minutes</label>' +
      '<input type="radio" id="chartBox'+boxCount+'hours" name="boxGranularity'+boxCount+'" value="hours"/>' +
      '<label for="chartBox'+boxCount+'hours" class="cb-disable">Hours</label>' +
      '<input type="radio" id="chartBox'+boxCount+'days" name="boxGranularity'+boxCount+'" value="days"/>' +
      '<label for="chartBox'+boxCount+'days" class="cb-disable">Days</label>'+ 
    '</form>'); 

    $.each(i, function(key, val ) {
      $('#chartBox'+boxCount+i.granularity).click();
      var grantouse = $('input[name=boxGranularity'+boxCount+']:checked').val(); 
      if ( key == "type" ) {
          switch (val) {
            case "connection":
              $("#chartBox"+boxCount+" .greydiv").append('<a class="statsHeader">'+i.type+'</a><a class="statsHeaderGran">('+grantouse+')</a>');
              drawConnections('#placeholderBox'+boxCount, grantouse); 
              break; 
            case "request":
              $("#chartBox"+boxCount+" .greydiv").append('<a class="statsHeader">'+i.type+'</a><a class="statsHeaderGran">('+grantouse+')</a>');
              $("#placeholderBoxSettings"+boxCount).append('<p class="requestChoices" >' +
                '<input id="sent'+boxCount+'" type="checkbox" name="bytessent'+boxCount+'">'+
                '<label for="sent'+boxCount+'">Bytes sent</label>'+
                '<br>'+
                '<input id="received'+boxCount+'" type="checkbox" name="bytesreceived'+boxCount+'">'+
                '<label for="idreceived'+boxCount+'" >Bytes received</label>'+          
              '</p>');

              if (i.sent == true) {
                $('#sent'+boxCount).click(); 
              }
              if (i.received == true) {
                $('#received'+boxCount).click(); 
              } 

              drawRequests('#placeholderBox'+boxCount, grantouse); 
              break; 
          }
      } 
    });
  });
  makeDraggableAndResizable (); 
  $(".radioFormat").buttonset();
}

function updateChartBoxes () {
  var boxCount; 
  $.each(existingCharts, function(v, i ) {
    boxCount = i.id;  
    $.each(i, function(key, val ) {
      var grantouse = $('input[name=boxGranularity'+boxCount+']:checked').val(); 
      if ( key == "type" ) {
        switch(val) { 
          case "connection":
            $('#chartBox'+boxCount+' .statsHeaderGran').html("("+grantouse+")"); 
            drawConnections('#placeholderBox'+boxCount, grantouse); 
            break; 
          case "request": 
            $('#chartBox'+boxCount+' .statsHeaderGran').html("("+grantouse+")"); 
            drawRequests('#placeholderBox'+boxCount, grantouse); 
            break;
        }
      }
    });
  });
}

function makeDraggableAndResizable () {
      $( ".resizable" ).resizable({
        grid: 10, 
        stop: function (event, ui) {
          stateSaving(); 
        } 
      }); 
      $( ".draggable" ).draggable({
        grid: [ 10,10 ],  
        containment: "#centerView",  
        stop: function (event, ui) {
          stateSaving(); 
        }
      }); 
}

var temptop = 150; 
var templeft = 20; 

function createSingleBox (id, val, question) {
  var boxCount = id;  

  $("#graphBox").append('<div id="chartBox'+boxCount+'" class="chartContainer resizable draggable"/>'); 
  $("#chartBox"+boxCount).css({top: temptop, left: templeft}); 
  $("#chartBox"+boxCount).append('<div class="greydiv"/>');
  $("#chartBox"+boxCount).append('<div class="placeholderBox" id="placeholderBox'+boxCount+'"/>');
  $("#chartBox"+boxCount).append('<div class="placeholderBoxSettings" id="placeholderBoxSettings'+boxCount+'" style="display:none"/>');
  $("#chartBox"+boxCount+" .greydiv").append('<button class="statsClose"><img width="16" height="16" src="/_admin/html/media/icons/whitedelete_icon16.png"></button>');
  $("#chartBox"+boxCount+" .greydiv").append('<button class="statsSettings"><img width="16" height="16" src="/_admin/html/media/icons/settings_icon16.png"></button>');
  $("#chartBox"+boxCount+" .greydiv").append('<button class="statsCharts"><img width="16" height="16" src="/_admin/html/media/icons/whitechart_icon16.png"></button>');

  $("#chartBox"+boxCount+" .placeholderBoxSettings").append('<form action="#" id="graphForm'+boxCount+'" class="radioFormat">' +
    '<input type="radio" id="chartBox'+boxCount+'minutes" name="boxGranularity'+boxCount+'" value="minutes" checked/>' +
    '<label for="chartBox'+boxCount+'minutes" class="cb-enable selected">Minutes</label>' +
    '<input type="radio" id="chartBox'+boxCount+'hours" name="boxGranularity'+boxCount+'" value="hours"/>' +
    '<label for="chartBox'+boxCount+'hours" class="cb-disable">Hours</label>' +
    '<input type="radio" id="chartBox'+boxCount+'days" name="boxGranularity'+boxCount+'" value="days"/>' +
    '<label for="chartBox'+boxCount+'days" class="cb-disable">Days</label>'+ 
    '</form>'); 

    switch (val) {
      case "connection":
        $("#chartBox"+boxCount+" .greydiv").append('<a class="statsHeader">'+val+'</a><a class="statsHeaderGran">(minutes)</a>');
        drawConnections('#placeholderBox'+boxCount, "minutes"); 
        break; 
      case "request":
        $("#chartBox"+boxCount+" .greydiv").append('<a class="statsHeader">'+val+'</a><a class="statsHeaderGran">(minutes)</a>');
        $("#placeholderBoxSettings"+boxCount).append('<p class="requestChoices" >' +
          '<input id="sent'+boxCount+'" type="checkbox" checked="checked" name="bytessent'+boxCount+'">'+
          '<label for="sent'+boxCount+'">Bytes sent</label>'+
          '<br>'+
          '<input id="received'+boxCount+'" checked="checked" type="checkbox" name="bytesreceived'+boxCount+'">'+
          '<label for="idreceived'+boxCount+'" >Bytes received</label>'+          
        '</p>');

        if (question == true) {

        }
        else {
          $('#sent'+boxCount).click(); 
          $('#received'+boxCount).click(); 
        }
        drawRequests('#placeholderBox'+boxCount, "minutes"); 
        break; 
    }
  $('#placeholderBoxSettings'+boxCount).append('<button id="saveChanges'+boxCount+'" class="saveChanges"></button>');
  makeDraggableAndResizable(); 
  $(".radioFormat").buttonset();
  templeft = templeft + 10; 
  temptop = temptop + 10;
  stateSaving();  
}

function highlightNavButton (buttonID) {
  $("#Collections").css('background-color', '#E3E3E3'); 
  $("#Collections").css('color', '#333333'); 
  $("#Query").css('background-color', '#E3E3E3'); 
  $("#Query").css('color', '#333333'); 
  $("#AvocSH").css('background-color', '#E3E3E3'); 
  $("#AvocSH").css('color', '#333333'); 
  $("#Logs").css('background-color', '#E3E3E3'); 
  $("#Logs").css('color', '#333333'); 
  $("#Status").css('background-color', '#E3E3E3'); 
  $("#Status").css('color', '#333333'); 
  $(buttonID).css('background-color', '#9EAF5A'); 
  $(buttonID).css('color', 'white'); 
}

function getCollectionInfo (identifier) {
  var collectionInfo = { };

  $.ajax({
    type: "GET",
    url: "/_api/collection/" + identifier + "?" + getRandomToken(),
    contentType: "application/json",
    processData: false,
    async: false,  
    success: function(data) {
      collectionInfo = data;
    },
    error: function(data) {
    }
  });

  return collectionInfo;
}
  
function loadDocuments (collectionName, currentPage) {
  var documentsPerPage = 10;
  var documentCount = 0;
  var totalPages;

  $.ajax({
    type: "GET",
    url: "/_api/collection/" + collectionName + "/count?" + getRandomToken(), 
    contentType: "application/json",
    processData: false,
    async: false,  
    success: function(data) {
      globalCollectionName = data.name;
      totalPages = Math.ceil(data.count / documentsPerPage);
      documentCount = data.count; 
      $('#nav2').text(globalCollectionName);
    },
    error: function(data) {
    }
  });

  if (isNaN(currentPage) || currentPage == undefined || currentPage < 1) {
    currentPage = 1;
  }

  if (totalPages == 0) {
    totalPages = 1;
  }

  // set the global variables, argl!
  collectionCurrentPage = currentPage; 
  collectionTotalPages = totalPages;

  var offset = (currentPage - 1) * documentsPerPage; // skip this number of documents
  $('#documentsTableID').dataTable().fnClearTable();

  $.ajax({
    type: 'PUT',
    url: '/_api/simple/all/',
    data: '{"collection":"' + collectionName + '","skip":' + offset + ',"limit":' + String(documentsPerPage) + '}', 
    contentType: "application/json",
    success: function(data) {
      $.each(data.result, function(k, v) {
        $('#documentsTableID').dataTable().fnAddData(['<button class="enabled" id="deleteDoc"><img src="/_admin/html/media/icons/doc_delete_icon16.png" width="16" height="16"></button><button class="enabled" id="editDoc"><img src="/_admin/html/media/icons/doc_edit_icon16.png" width="16" height="16"></button>', v._id, v._key, v._rev, '<pre class=prettify>' + cutByResolution(JSON.stringify(v)) + '</pre>' ]);  
      });
      $(".prettify").snippet("javascript", {style: "nedit", menu: false, startText: false, transparent: true, showNum: false});
      $('#documents_status').text(String(documentCount) + " document(s), showing page " + currentPage + " of " + totalPages); 
    },
    error: function(data) {
      
    }
  });
}

function systemAttributes () {
  return {
    '_id'            : true, 
    '_rev'           : true, 
    '_key'           : true, 
    '_from'          : true, 
    '_to'            : true, 
    '_bidirectional' : true,
    '_vertices'      : true,
    '_from'          : true, 
    '_to'            : true, 
    '$id'            : true 
  };
}

function isSystemAttribute (val) {
  var a = systemAttributes();
  return a[val];
}

function isSystemCollection (val) {
  return val && val.name && val.name.substr(0, 1) === '_';
}

function collectionApiType (identifier) {
  if (CollectionTypes[identifier] == undefined) {
    CollectionTypes[identifier] = getCollectionInfo(identifier).type;
  }

  if (CollectionTypes[identifier] == 3) {
    return "edge";
  }
  return "document";
}

function collectionType (val) {
  if (! val || val.name == '') {
    return "-";
  }

  var type;
  if (val.type == 2) {
    type = "document";
  }
  else if (val.type == 3) {
    type = "edge";
  }
  else {
    type = "unknown";
  }

  if (isSystemCollection(val)) {
    type += " (system)";
  }

  return type;
}

function getErrorMessage (data) {
  if (data && data.responseText) {
    // HTTP error
    try {
      var json = JSON.parse(data.responseText);
      if (json.errorMessage) {
        return json.errorMessage;
      }
    }
    catch (e) {
    }

    return data.responseText;
  }
    
  if (data && data.responseText === '') {
    return "Could not connect to server";
  }

  if (typeof data == 'string') {
    // other string error
    return data;
  }

  if (data != undefined) {
    // some other data type
    return JSON.stringify(data);
  }

  // fallback
  return "an unknown error occurred";
}

function getRandomToken ()  {
  return Math.round(new Date().getTime());
}

