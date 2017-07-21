// Try to set the version number early, jQuery not available yet
document.addEventListener("DOMContentLoaded", function(event) {
  if (!gitbook.state.root) return;
  var bookVersion = gitbook.state.root.match(/\/(\d\.\d(\.\d)?|devel)\//);
  var switcher = document.getElementsByClassName("arangodb-version-switcher")[0];
  if (bookVersion) {
    switcher.value = bookVersion[1];
  } else {
    switcher.style.display = "none";
  }
});

window.onload = function(){
window.localStorage.removeItem(":keyword");

$(document).ready(function() {

function appendHeader() {

  var div = document.createElement('div');
  div.innerHTML = '<div class="arangodb-header">\n' +
    '  <div class="arangodb-logo">\n' +
    '    <a href="https://arangodb.com/">\n' +
    '      <img src="https://docs.arangodb.com/assets/arangodb_logo_2016.png">\n' +
    '    </a>\n' +
    '  </div>\n' +
    '  <div class="arangodb-logo-small">\n' +
    '    <a href="https://arangodb.com/">\n' +
    '      <img src="https://docs.arangodb.com/assets/arangodb_logo_small_2016.png">\n' +
    '    </a>\n' +
    '  </div>\n' +
    '  <select class="arangodb-version-switcher">\n' +
    '    <option value="devel">devel</option>\n' +
    '    <option value="3.2">v3.2</option>\n' +
    '    <option value="3.1">v3.1</option>\n' +
    '    <option value="3.0">v3.0</option>\n' +
    '    <option value="2.8">v2.8</option>\n' +
    '  </select>\n' +
    '  <div class="google-search">\n' +
    '    <gcse:searchbox-only></gcse:searchbox-only>\n' +
    '  </div>\n' +
    '  <ul class="arangodb-navmenu">\n' +
    '    <li>\n' +
    '      <a href="#" data-book="Manual">Manual</a>\n' +
    '    </li>\n' +
    '    <li>\n' +
    '      <a href="#" data-book="AQL">AQL</a>\n' +
    '    </li>\n' +
    '    <li class="active-tab">\n' +
    '      <a href="#" data-book="HTTP">HTTP</a>\n' +
    '    </li>\n' +
    '    <li>\n' +
    '      <a href="#" data-book="cookbook">Cookbook</a>\n' +
    '    </li>\n' +
    '    <li class="downloadIcon" title="Download">\n' +
    '      <a href="https://www.arangodb.com/download-arangodb-docs" target="_blank">\n' +
    '        <i class="fa fa-download"></i>\n' +
    '      </a>\n' +
    '    </li>\n' +
    '    <li class="socialIcons" title="GitHub">\n' +
    '      <a href="https://github.com/ArangoDB/ArangoDB/issues" target="_blank">\n' +
    '        <i class="fa fa-github"></i>\n' +
    '      </a>\n' +
    '    </li>\n' +
    '    <li class="socialIcons" title="StackOverflow">\n' +
    '      <a href="http://stackoverflow.com/questions/tagged/arangodb" target="_blank">\n' +
    '        <i class="fa fa-stack-overflow"></i>\n' +
    '      </a>\n' +
    '    </li>\n' +
    '    <li class="socialIcons socialIcons-googlegroups" title="Google Groups">\n' +
    '      <a href="https://groups.google.com/forum/#!forum/arangodb" target="_blank">\n' +
    '        <img alt="Google Groups" src="https://docs.arangodb.com/assets/googlegroupsIcon.png" />\n' +
    '      </a>\n' +
    '    </li>\n' +
    '    <li class="socialIcons" title="Slack">\n' +
    '      <a href="https://slack.arangodb.com" target="_blank">\n' +
    '        <i class="fa fa-slack"></i>\n' +
    '      </a>\n' +
    '    </li>\n' +
    '  </ul>\n' +
    '</div>\n';

    $('.book').before(div.innerHTML);

  };


  function rerenderNavbar() {
    $('.arangodb-header').remove();
    appendHeader();
  };

  //render header
  rerenderNavbar();
  function addGoogleSrc() {
    var cx = '010085642145132923492:fixi4yzeiz8';
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

  // set again using jQuery to accommodate non-standard browsers (*cough* IE *cough*)
  var bookVersion = gitbook.state.root.match(/\/(\d\.\d(\.\d)?|devel)\//);
  var switcher = $(".arangodb-version-switcher");
  if (bookVersion) {
    switcher.val(bookVersion[1]);
  } else {
    switcher.hide();
  }
  
  $(".arangodb-version-switcher").on("change", function(e) {
    var urlSplit = gitbook.state.root.split("/");
    urlSplit.pop(); // ""
    var currentBook = urlSplit.pop(); // e.g. "Manual"
    urlSplit.pop() // e.g. "3.0"
    if (e.target.value == "2.8") {
      var legacyMap = {
        "Manual": "",
        "AQL": "/Aql",
        "HTTP": "/HttpApi",
        "cookbook": "/cookbook"
      };
      currentBook = legacyMap[currentBook];
    } else {
      currentBook = "/" + currentBook;
    }
    window.location.href = urlSplit.join("/") + "/" + e.target.value + currentBook + "/index.html";
  });

});

};
