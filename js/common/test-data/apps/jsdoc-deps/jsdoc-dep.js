'use strict';
var i = 0;

module.exports = {
  /** This
   * is
   * a
   * multiline
   * comment
   */
  a: i++,

  /** this is all inline */
  b: i++,

  /** A comment
     without stars
  */
  c: i++,

  /** A comment
   Containing 2 * 3 = 5
   */
  e: i++,

  /** Slashy // comments
   *
   * Are evil
   */
  f: i++,

  /** just
   * some random comment
   */
  /** What if
   the style of
   * comments
   is changeing
   All the time */
  g: i++,

  /** I am
   * / doing funny things
   */
  h: i++,

  /* This should
   * be ignored
   */
  i: i++,

  // This is also ignored
  j: i++,


/** Crazy
             * indention
 * of
stuff     */
  k: i++,

  /** This
   * "is" 
   * With quotation '' marks
   */
  l: i++,

  /** valid
   * comment
   */
  m: i++
    /** this
     * shall be ignored
     */
};
