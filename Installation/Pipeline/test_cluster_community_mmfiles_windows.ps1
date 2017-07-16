. Installation\Pipeline\include\test_setup_tmp.ps1
. Installation\Pipeline\port.ps1
. Installation\Pipeline\include\test_MODE_EDITION_ENGINE_windows.ps1

Move-Item -force .\build\bin\RelWithDebInfo\* .\build\bin\

$result = RunTests -port $port -engine mmfiles -edition community -mode cluster

del $portFile

exit $result
