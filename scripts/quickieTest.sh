#!/bin/bash
scripts/unittest shell_server --test js/common/tests/shell/shell-quickie.js
scripts/unittest shell_server --test js/common/tests/shell/shell-quickie.js --cluster true --agencySupervision false
scripts/unittest shell_client --test js/common/tests/shell/shell-quickie.js
scripts/unittest shell_client --test js/common/tests/shell/shell-quickie.js --cluster true --agencySize 1
