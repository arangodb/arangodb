(function() {
  var oldAjax = $.ajax;
  $.ajax = function(url, obj) {
    var oldURL,
        serverPrefix = "http://localhost:7777";
    if (typeof url === "object") {
      if (url.url.charAt(0) === '/') {
        url.url = serverPrefix + url.url;
      }
      return oldAjax(url);
    }
    if (url.charAt(0) === '/') {
      url = serverPrefix + url;
    }
    return oldAjax(url, obj);
  };
}());
