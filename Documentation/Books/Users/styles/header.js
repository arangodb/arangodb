window.onload = function(){
window.localStorage.removeItem(":keyword");

$(document).ready(function() {

function appendHeader() {

  var div = document.createElement('div');
  div.innerHTML = '<header id="header" class="header absolute"><div class="wrap"><div class="clearfix" style="width:100%;"><div id="logo"><a href="https://docs.arangodb.com/"><img src="https://docs.arangodb.com/assets/arangodb_logo.png"></a></div><div class="arangodb_version">VERSION_NUMBER</div><div class="google-search"><gcse:searchbox-only></div><ul id="navmenu"><li><a href="https://www.arangodb.com">Website</a></li><li class="owntab"><a href="https://docs.arangodb.com">Documentation</a></li><li><a href="https://docs.arangodb.com/cookbook">Cookbook</a></li><li class="socialIcons"><a href="https://github.com/ArangoDB/ArangoDB/issues" target="blank" name="github"><i title="GitHub" class="fa fa-github"></i></a></li><li class="socialIcons"><a href="http://stackoverflow.com/questions/tagged/arangodb" target="blank" name="stackoverflow"><i title="Stackoverflow" class="fa fa-stack-overflow"></i></a></li><li class="socialIcons socialIcons-googlegroups"><a href="https://groups.google.com/forum/#!forum/arangodb" target="blank" name="google groups"><img title="Google Groups" alt="Google Groups" src="https://docs.arangodb.com/assets/googlegroupsIcon.png" style="height:14px"></img></a></li></ul></div></div></header>';

    $('.book').before(div.innerHTML);

  };


  function rerenderNavbar() {
    $('#header').remove();
    appendHeader();
    renderGoogleSearch();
  };

  function renderGoogleSearch() {
  };
  //render header
  rerenderNavbar();
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

});

};
