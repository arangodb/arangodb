window.onload = function(){

function appendHtml(el) {

  var div = document.createElement('div');
  div.innerHTML = '<header id="header" class="header fixed"><div class="wrap"><div class="clearfix" style="width:100%;"><div id="logo"><img src="https://docs.arangodb.com/assets/arangodb_logo.png"></div><ul id="navmenu"><li><a href="https://www.arangodb.com">Website</a></li><li class="owntab"><a href="https://docs.arangodb.com">Documentation</a></li><li><a href="https://docs.arangodb.com/cookbook">Cookbook</a></li><li class="socialIcons"><a href="https://github.com/triAGENS/ArangoDB/issues" target="blank" name="github"><i class="fa fa-github"></i></a></li><li class="socialIcons"><a href="http://stackoverflow.com/questions/tagged/arangodb" target="blank" name="stackoverflow"><i class="fa fa-stack-overflow"></i></a></li><li class="socialIcons"><a href="https://groups.google.com/forum/#!forum/arangodb" target="blank" name="google groups"><img src="https://docs.arangodb.com/assets/googlegroupsIcon.png" style="height:14px"></img></a></li></ul></div></div></header><div id="spacer" class="spacerx"></div><div id="searchx"></div>';//<gcse:search></gcse:search>
  el.insertBefore(div,el.childNodes[0]); 
}

(function() {
  var cx = '002866056653122356950:ju52xx-w-w8';
  var gcse = document.createElement('script');
  gcse.type = 'text/javascript';
  gcse.async = true;
  gcse.src = (document.location.protocol == 'https:' ? 'https:' : 'http:') +
    '//cse.google.com/cse.js?cx=' + cx;
  var s = document.getElementsByTagName('script')[0];
  s.parentNode.insertBefore(gcse, s);
})();

appendHtml(document.body);

var header = document.querySelector("#header");

};
