IF NOT DEFINED CXXSTD (
ECHO %ARGS:"=%
..\..\..\b2 config_info_travis_install %ARGS:"=%
config_info_travis
del config_info_travis.exe
)
IF DEFINED CXXSTD FOR %%A IN (%CXXSTD%) DO (
ECHO %ARGS:"=%
..\..\..\b2 -a -d2 config_info_travis_install %ARGS:"=% cxxstd=%%A
config_info_travis
del config_info_travis.exe
)