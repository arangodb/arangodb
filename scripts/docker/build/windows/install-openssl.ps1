param (
  [Parameter(Mandatory=$true)][string]$branch,
  [Parameter(Mandatory=$true)][string]$revision
)

$ErrorActionPreference = "Stop"

$OPENSSL_BRANCH=$branch
$OPENSSL_REVISION=$revision
$OPENSSL_VERSION="${OPENSSL_BRANCH}${OPENSSL_REVISION}"

$OPENSSL_TAG="openssl-$OPENSSL_VERSION"
$BUILD_FOLDER="C:\OpenSSL-build\"

git clone --depth 1 --branch $OPENSSL_TAG https://github.com/openssl/openssl $BUILD_FOLDER
if ($LASTEXITCODE -ne 0) { throw "git clone failed" }
Set-Location $BUILD_FOLDER

$INSTALL_DIR = "C:\OpenSSL-$OPENSSL_VERSION\"
perl Configure no-async no-shared VC-WIN64A --release --prefix=${INSTALL_DIR} --openssldir=${INSTALL_DIR}\ssl
if ($LASTEXITCODE -ne 0) { throw "configure failed" }

$buildCommand = "call `"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat`" -arch=amd64 && set CL=/MP && nmake && nmake install"
Invoke-Expression "& cmd /c '$buildCommand'"

Set-Location "C:\"
Remove-Item $BUILD_FOLDER -Recurse -Force
