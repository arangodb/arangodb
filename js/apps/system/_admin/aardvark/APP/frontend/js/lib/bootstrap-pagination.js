/**
 * * bootstrap-pagination.js by takumakei (http://github.com/takumakei)
 * * Copyright 2012 takumakei.
 * * http://www.apache.org/licenses/LICENSE-2.0.txt
 * */
(function ($) {
  'use strict';
  function defaultClick(i) {}
  function stopPropagation(event) { event.stopPropagation(); }
  $.fn.pagination = function (config) {
    var options = $.extend({
      prev: '<span><i class="fa fa-angle-left"/></span>',
      next: '<span><i class="fa fa-angle-right"/></span>',
      left: 3,
      right: 3,
      page: 1,
      lastPage: 1,
      click: defaultClick
    }, config || {}),
    lr = options.left + options.right,
    begin = Math.max(1, options.page - options.left),
    end = begin + lr;
    if (options.lastPage < end) {
      begin = Math.max(1, options.lastPage - lr);
      end = options.lastPage;
    }
    function newLI(label, page) {
      var enable = (1 <= page && page <= options.lastPage),
      a = $('<a/>').append(label),
      li = $('<li/>').append(a);
      if (enable) {
        a.click(function () { options.click(page); });
        $('.arango-pagination').next().children().first().removeClass('disabledPag');
        $('.arango-pagination').prev().children().first().removeClass('disabledPag');
      } else {
        li.addClass('disabledPag');

        setTimeout(function(){
          if ($(li).find('i').hasClass('fa-angle-right')) {
            $('.arango-pagination').next().children().first().addClass('disabledPag');
          }
          else if ($(li).find('i').hasClass('fa-angle-left')) {
            $('.arango-pagination').prev().children().first().addClass('disabledPag');
          }
        }, 50);

      }
      a.click(stopPropagation);
      if (page === options.page) { li.addClass('active active-arango-pagination-button'); }
      return li;
    }
    function newUL() {
      var ul = $('<ul class="arango-pagination"/>'), i;
      ul.append(newLI(options.prev, options.page - 1));
      for (i = begin; i <= end; i += 1) { ul.append(newLI(i, i)); }
      return ul.append(newLI(options.next, options.page + 1));
    }
    return this.each(function (i) {
      var self = $(this), ul = self.children('ul');
      if (ul.length) {
        ul.replaceWith(newUL());
      } else {
        self.append(newUL());
      }
    });
  };
}(jQuery));
