var ModuleCache = { "/internal" : { "exports": { } } };

var SYS_START_PAGER = function() { };
var SYS_STOP_PAGER = function() { };
var TRI_SYS_OUTPUT = function() { };

var print = function (value) {
  hansmann(value); 
};

function ArangoConnection () {
  
}

var arango = new ArangoConnection();

ArangoConnection.prototype.get = function (url) {
var msg; 
  $.ajax({
    async: false, 
    type: "GET",
    url: url, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      msg = data; 
    }
  });
  return msg;  
};

ArangoConnection.prototype.delete = function (url) {
var msg; 
  $.ajax({
    async: false, 
    type: "DELETE",
    url: url, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = JSON.stringify(data); 
    },
    error: function(data) {
      msg = JSON.stringify(data);  
    }
  });
  return msg;  
};


ArangoConnection.prototype.post = function (url, body) {
var msg;
  $.ajax({
    async: false, 
    type: "POST",
    url: url, 
    data: body, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
      msg = data; //JSON.stringify(data); 
    },
    error: function(data) {
      msg = JSON.stringify(data);  
    }
  });
  return msg;  
};


ArangoConnection.prototype.put = function (url, body) {
var msg; 
  $.ajax({
    async: false, 
    type: "PUT",
    url: url, 
    data: body, 
    contentType: "application/json",
    processData: false, 
    success: function(data) {
return data;
      msg = JSON.stringify(data); 
    },
    error: function(data) {
      msg = JSON.stringify(data);  
    }
  });
  return msg;  
};

ArangoConnection.prototype.GET = ArangoConnection.prototype.get;
ArangoConnection.prototype.POST = ArangoConnection.prototype.post;
ArangoConnection.prototype.DELETE = ArangoConnection.prototype.delete;
ArangoConnection.prototype.PUT = ArangoConnection.prototype.put;
