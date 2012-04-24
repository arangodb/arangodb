////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.log = console;

function tableTestSuite () {

  function testDerHans () {
    jsUnity.assertions.assertEqual("string: peng", value2html("peng"));
    jsUnity.assertions.assertEqual("string: 25", value2html("25"));
    jsUnity.assertions.assertEqual("number: 25", value2html(25));
    jsUnity.assertions.assertFalse(false);
    jsUnity.assertions.assertTrue(!!true);
  }
/*
  function testFleisch () {
    //jsUnity.assertions.assertEqual("string: peng sexmann", value2html("peng"));  
  }
*/

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(tableTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
