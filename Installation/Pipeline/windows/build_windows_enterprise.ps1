$ErrorActionPreference="Stop"
$vcpath=$(Get-ItemProperty HKLM:\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VC7)."14.0"
#$env:_MSPDBSRV_ENDPOINT_="enterprise-${env:BUILD_TAG}"
$buildOptions = "-DUSE_MAINTAINER_MODE=On -DUSE_ENTERPRISE=On -DUSE_CATCH_TESTS=On -DUSE_FAILURE_TESTS=On -DDEBUG_SYNC_REPLICATION=On -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSKIP_PACKAGING=On"
Remove-Item -Force -Recurse log-output -ErrorAction SilentlyContinue
New-Item -Force -ItemType Directory log-output -ErrorAction SilentlyContinue
if (Get-Command docker -errorAction SilentlyContinue) {
  $buildOptions += " -DOPENSSL_INCLUDE_DIR=`"`$env:OPENSSL_INCLUDE_DIR`" -DLIB_EAY_RELEASE=`"`$env:LIB_EAY_RELEASE`" -DSSL_EAY_RELEASE=`"`$env:SSL_EAY_RELEASE`" -DLIB_EAY_RELEASE_DLL=`"`$env:LIB_EAY_RELEASE_DLL`" -DSSL_EAY_RELEASE_DLL=`"`$env:SSL_EAY_RELEASE_DLL"
  $volume = "$env:WORKSPACE"
  $volume += ":C:\arangodb"
  $build = @'
$ErrorActionPreference="Stop"
New-Item -ItemType Directory -Force -Path c:\arangodb\build
cd c:\arangodb\build
cmake .. -G "Visual Studio 14 2015 Win64" ${buildOptions}
cmake --build . --config RelWithDebInfo
exit $LastExitCode
'@
  $build > buildscript.ps1

  docker run --rm -v $volume m0ppers/build-container powershell C:\arangodb\buildscript.ps1 | Set-Content -PassThru log-output\build.log
} else {
  $env:GYP_MSVS_OVERRIDE_PATH="${vcpath}\bin"
  New-Item -ItemType Directory -Force -Path build
  cd build
  Invoke-Expression "cmake .. -G `"Visual Studio 15 2017 Win64`" ${buildOptions} | Set-Content -PassThru ..\log-output\build.log"
  cmake --build . --config RelWithDebInfo | Add-Content -PassThru ..\log-output\build.log
  cd ..
}