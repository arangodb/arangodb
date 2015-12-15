(function() {
  'use strict';
  var FoxxApplication = require("@arangodb/foxx").Controller;
  var controller = new FoxxApplication(applicationContext);


  /** This
   * is
   * a
   * multiline
   * comment
   */
  controller.get("/a", function() {});

  /** this is all inline */
  controller.get("/b", function() {});

  /** A comment
     without stars
  */
  controller.get("/c", function() {});

  /** A comment
   Containing 2 * 3 = 5
   */
  controller.get("/e", function() {});

  /** Slashy // comments
   *
   * Are evil
   */
  controller.get("/f", function() {});

  /** just
   * some random comment
   */
  /** What if
   the style of
   * comments
   is changeing
   All the time */
  controller.get("/g", function() {});

  /** I am
   * / doing funny things
   */
  controller.get("/h", function() {});

  /* This should
   * be ignored
   */
  controller.get("/i", function() {});

  // This is also ignored
  controller.get("/j", function() {});


/** Crazy
             * indention
 * of
stuff     */
  controller.get("/k", function() {});

  /** This
   * "is" 
   * With quotation '' marks
   */
  controller.get("/l", function() {});

  /** valid
   * comment
   */
  controller.get("/m", function() {
    /** this
     * shall be ignored
     */
  });
}());
