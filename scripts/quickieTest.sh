#!/bin/bash
echo ===============================================================
echo Note that it is expected that this cluster test writes warnings
echo about termination signals to V8 contexts, please ignore!
echo ===============================================================
scripts/unittest shell_server --test js/common/tests/shell/shell-quickie.js
scripts/unittest shell_server --test js/common/tests/shell/shell-quickie.js --cluster true
scripts/unittest shell_client --test js/common/tests/shell/shell-quickie.js
scripts/unittest shell_client --test js/common/tests/shell/shell-quickie.js --cluster true
echo ===============================================================
echo Note that it is expected that this cluster test writes warnings
echo about termination signals to V8 contexts, please ignore!
echo ===============================================================
