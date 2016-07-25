/* jshint browser: true */
/* jshint unused: false */
/* global Backbone, $, window */

(function () {
  'use strict';
  window.PaginationView = Backbone.View.extend({

    // Subclasses need to overwrite this
    collection: null,
    paginationDiv: '',
    idPrefix: '',

    rerender: function () {},

    jumpTo: function (page) {
      this.collection.setPage(page);
      this.rerender();
    },

    firstPage: function () {
      this.jumpTo(1);
    },

    lastPage: function () {
      this.jumpTo(this.collection.getLastPageNumber());
    },

    firstDocuments: function () {
      this.jumpTo(1);
    },
    lastDocuments: function () {
      this.jumpTo(this.collection.getLastPageNumber());
    },
    prevDocuments: function () {
      this.jumpTo(this.collection.getPage() - 1);
    },
    nextDocuments: function () {
      this.jumpTo(this.collection.getPage() + 1);
    },

    renderPagination: function () {
      $(this.paginationDiv).html('');
      var self = this;
      var currentPage = this.collection.getPage();
      var totalPages = this.collection.getLastPageNumber();
      var target = $(this.paginationDiv);
      var options = {
        page: currentPage,
        lastPage: totalPages,
        click: function (i) {
          var split = window.location.hash.split('/');
          if (split[2] === 'documents') {
            options.page = i;
            window.location.hash = split[0] + '/' + split[1] + '/' + split[2] + '/' + i;
          } else {
            self.jumpTo(i);
            options.page = i;
          }
        }
      };
      target.html('');
      target.pagination(options);
      $(this.paginationDiv).prepend(
        '<ul class="pre-pagi"><li><a id="' + this.idPrefix +
        '_first" class="pagination-button">' +
        '<span><i class="fa fa-angle-double-left"/></span></a></li></ul>'
      );
      $(this.paginationDiv).append(
        '<ul class="las-pagi"><li><a id="' + this.idPrefix +
        '_last" class="pagination-button">' +
        '<span><i class="fa fa-angle-double-right"/></span></a></li></ul>'

      );
    }

  });
}());
