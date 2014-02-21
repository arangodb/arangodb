function main (argv) {
  var print = require("internal").print;
  if (argv.length < 2) {
    print("Usage: unittest TESTNAME [OPTIONS]");
    return;
  }
  var test = argv[1];
  var options = {};
  if (argv.length >= 3) {
    options = JSON.parse(argv[2]);
  }
  var UnitTest = require("org/arangodb/testing").UnitTest;
  start_pretty_print();
  print(UnitTest(test,options));
}
