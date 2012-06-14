////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.log = console;

function tableTestSuite () {

////////////////////////////////////////////////////////////////////////////////
/// @brief value2Html function
////////////////////////////////////////////////////////////////////////////////

  function test_value2Html () {
    jsUnity.assertions.assertEqual("<a class=sh_string>peng</a>", value2html("peng"));
    jsUnity.assertions.assertEqual("<a class=sh_string>25</a>", value2html("25"));
    jsUnity.assertions.assertEqual("<a class=sh_number>25</a>", value2html(25));
    jsUnity.assertions.assertFalse(false);
    jsUnity.assertions.assertTrue(!!true);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief is_int function 
////////////////////////////////////////////////////////////////////////////////

  function test_is_int () {
    jsUnity.assertions.assertEqual(true, is_int(25));
    jsUnity.assertions.assertEqual(false, is_int(25.5));
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief bytesToSize function
////////////////////////////////////////////////////////////////////////////////

  function test_bytesToSize() {
    jsUnity.assertions.assertEqual("1 B", bytesToSize(1));
    jsUnity.assertions.assertEqual("1 KB", bytesToSize(1024));
    jsUnity.assertions.assertEqual("1 MB", bytesToSize(1048576));
    jsUnity.assertions.assertEqual("1 GB", bytesToSize(1073741824));
    jsUnity.assertions.assertEqual("1 TB", bytesToSize(1099511627776));
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief RealTypeOf function
////////////////////////////////////////////////////////////////////////////////

  function test_RealTypeOf() {
    jsUnity.assertions.assertEqual("string", RealTypeOf("hehe"));
    jsUnity.assertions.assertEqual("number", RealTypeOf(123));
    jsUnity.assertions.assertEqual("string", RealTypeOf("123"));
    jsUnity.assertions.assertEqual("array", RealTypeOf([1, 2, 3]));
    jsUnity.assertions.assertEqual("object", RealTypeOf({"hehe":123}));
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief getTypedValue
////////////////////////////////////////////////////////////////////////////////

  function test_getTypedValue() {
    jsUnity.assertions.assertEqual(true, getTypedValue("true"));
    jsUnity.assertions.assertEqual(false, getTypedValue("false"));
    jsUnity.assertions.assertEqual(123, getTypedValue("123"));
    jsUnity.assertions.assertEqual("hehe", getTypedValue("hehe"));
    jsUnity.assertions.assertEqual(null, getTypedValue("null"));
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(tableTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
