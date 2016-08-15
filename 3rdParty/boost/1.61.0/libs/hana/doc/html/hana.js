// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

// We parse the code snippets and italicize the words from the pseudo-code
// glossary to make them stand out. We also link them to their respective
// definition in the glossary.
window.onload = function() {
  $(".fragment").children(".line").each(function(index, div) {
    div.innerHTML = div.innerHTML
      .replace(/perfect-.+(?=])/g, "perfect-capture".link("index.html#tutorial-glossary-perfect_capture").italics())
      .replace(/forwarded/g, "forwarded".link("index.html#tutorial-glossary-forwarded").italics())
      .replace(/tag-dispatched/g, "tag-dispatched".link("index.html#tutorial-glossary-tag_dispatched").italics())
      .replace(/implementation_defined/g, "implementation-defined".link("index.html#tutorial-glossary-implementation_defined").italics())
      .replace(/see-documentation/g, "see-documentation".italics());
  });

  $(".benchmark-chart").each(function(index, div) {
    var dataset = div.getAttribute("data-dataset");
    $.getJSON("benchmarks/release/clang-3.6.2/" + dataset, function(options) {
      Hana.initChart($(div), options);
    });
  });
};
