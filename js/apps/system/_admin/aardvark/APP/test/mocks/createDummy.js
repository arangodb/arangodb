/*jshint browser: true */
/*global spyOn, _ */

(function() {
  "use strict";

  var dummyFunction = function() {
    throw new Error("Function " + this + " should be a spy");
  };

  // Create a Dummy implementation of the object with
  // the given name bound to the parent.
  // Example:
  // CreateDummyForObject(window, "DocumentsView") will return a
  // Dummy view implementing all functions with replacements,
  // whenever executing x = new DocumentsView();
  window.CreateDummyForObject = function(par, objName) {
    var dummy = {};
    var fcts = _.functions(par[objName].prototype);
    var i;
    for (i = 0; i < fcts.length; ++i) {
      dummy[fcts[i]] = dummyFunction.bind(fcts[i]);
    }
    spyOn(par, objName).andReturn(dummy);
  };
}());
