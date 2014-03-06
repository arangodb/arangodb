echo off
rem for building the executables
rem type: "triagens-build.bat" 
rem the binaries etcd.exe and bench.exe
rem are written into the directory bin

set TARGET1=src\github.com\coreos
set TARGET2=src\github.com\coreos\etcd
echo %CD%
echo %TARGET2%
mkdir %TARGET2%

mkdir  %TARGET2%\config
XCOPY  config  %TARGET2%\config /E
mkdir  %TARGET2%\contrib
XCOPY  contrib  %TARGET2%\contrib /E
mkdir  %TARGET2%\discovery
XCOPY  discovery  %TARGET2%\discovery /E
mkdir  %TARGET2%\error
XCOPY  error  %TARGET2%\error /E
mkdir  %TARGET2%\http
XCOPY  http  %TARGET2%\http /E
mkdir  %TARGET2%\lo
XCOPY  lo  %TARGET2%\lo /E

mkdir  %TARGET2%\metrics
XCOPY  metrics  %TARGET2%\metrics /E

mkdir  %TARGET2%\mod
XCOPY  mod  %TARGET2%\mod /E

mkdir  %TARGET2%\pkg
XCOPY  pkg  %TARGET2%\pkg /E

mkdir  %TARGET2%\server
XCOPY  server  %TARGET2%\server /E

mkdir  %TARGET2%\store
XCOPY  store  %TARGET2%\store /E

mkdir  %TARGET2%\third_party
XCOPY  third_party %TARGET2%\third_party /E

mkdir  %TARGET2%\log
XCOPY  log %TARGET2%\log /E

XCOPY *.go %TARGET1%
set GOPATH=%CD%
set GOBIN=%CD%\bin

if exist  %GOBIN% goto WEITER
echo "warum bin ich hier?"
rd /S /Q %GOBIN%
:WEITER:
mkdir %GOBIN%
go install
chdir bench
go install
chdir ..
rd /S /Q src
dir bin
echo "etcd was build"
