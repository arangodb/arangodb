window.onload = function(){
window.localStorage.removeItem(":keyword");

$(document).ready(function() {

function appendHeader() {
/*
  var div = document.createElement('div');
  div.innerHTML = '<header id="header" class="header absolute"><div class="wrap"><div class="clearfix" style="width:100%;"><div id="logo"><a href="https://docs.arangodb.com/"><img src="https://docs.arangodb.com/assets/arangodb_logo.png"></a></div><div class="arangodb_version">VERSION_NUMBER</div><div class="google-search"><gcse:searchbox-only></div><ul id="navmenu"><li><a href="https://tst.arangodb.com/simran/all-in-one/">Docs</a></li><li><a href="https://docs.arangodb.com/cookbook">Cookbook</a></li><li class="socialIcons"><a href="https://github.com/ArangoDB/ArangoDB/issues" target="blank" name="github"><i title="GitHub" class="fa fa-github"></i></a></li><li class="socialIcons"><a href="http://stackoverflow.com/questions/tagged/arangodb" target="blank" name="stackoverflow"><i title="Stackoverflow" class="fa fa-stack-overflow"></i></a></li><li class="socialIcons socialIcons-googlegroups"><a href="https://groups.google.com/forum/#!forum/arangodb" target="blank" name="google groups"><img title="Google Groups" alt="Google Groups" src="https://docs.arangodb.com/assets/googlegroupsIcon.png" style="height:14px"></img></a></li></ul></div></div></header>';

    $('.book').before(div.innerHTML);
*/
  };


  function rerenderNavbar() {
    $('.arangodb-header').remove();
    appendHeader();
    renderGoogleSearch();
  };

  function renderGoogleSearch() {
  };
  //render header
  //rerenderNavbar();
  function addGoogleSrc() {
    var cx = '002866056653122356950:ju52xx-w-w8';
    var gcse = document.createElement('script');
    gcse.type = 'text/javascript';
    gcse.async = true;
    gcse.src = (document.location.protocol == 'https:' ? 'https:' : 'http:') +
        '//cse.google.com/cse.js?cx=' + cx;
    var s = document.getElementsByTagName('script')[0];
    s.parentNode.insertBefore(gcse, s);
  };
  addGoogleSrc();

  $(".arangodb-navmenu a:lt(4)").on("click", function(e) {
    e.preventDefault();
    var urlSplit = gitbook.state.root.split("/");
    urlSplit.pop(); // ""
    urlSplit.pop(); // e.g. "Manual"
    window.location.href = urlSplit.join("/") + "/" + e.target.getAttribute("data-book") + "/index.html";
  });

  var bookVersion = gitbook.state.root.match(/\/(\d\.\d|devel)\//);
  if (bookVersion) {
    $(".arangodb-version-switcher").val(bookVersion[1]);
  }
  
  $(".arangodb-version-switcher").on("change", function(e) {
    var urlSplit = gitbook.state.root.split("/");
    if (urlSplit.length == 6) {
      urlSplit.pop(); // ""
      var currentBook = urlSplit.pop(); // e.g. "Manual"
      urlSplit.pop() // e.g. "3.0"
      window.location.href = urlSplit.join("/") + "/" + e.target.value + "/" + currentBook + "/";
    } else {
      window.location.href = "https://docs.arangodb.com/" + e.target.value;
    }
  });

});

};
