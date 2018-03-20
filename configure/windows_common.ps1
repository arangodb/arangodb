#REQUIRES -Version 2.0
<#
.SYNOPSIS
    Common base of ArangoDB Windows build scripts.
.DESCRIPTION
    The Script Sets variables and executes CMake with a fitting
    generator and parameters required by the build. It optionally
    starts a build if the $do_build variable is set to $TRUE.
#>

# set variables
$ErrorActionPreference = "Continue"
$PSDefaultParameterValues['*:ErrorAction']='Continue'

$arango_source = split-path -parent $script_path

$registry_base = "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS"

if(Test-Path "${registry_base}\VC7") {
    $vcpath=$(Get-ItemProperty "${registry_base}\VC7")."$vc_version"
} elseif(Test-Path "${registry_base}\VS7") {
    $vcpath=$(Get-ItemProperty "${registry_base}\VS7")."$vc_version"
} else {
    echo "unable to find VisualStudio Registry Key"
    exit 1
}

$env:GYP_MSVS_OVERRIDE_PATH="${vcpath}\bin"
$env:CC="${vcpath}\bin\cl.exe"
$env:CXX="${vcpath}\bin\cl.exe"

# print configuration
echo "do build:               $do_build"
echo "extra args:             $Params"
echo "configuration:          $config"
echo "source path:            $arango_source"
echo "generator:              $generator"
echo "VC version:             $vc_version"
echo "CC:                     ${env:CC}"
echo "CXX:                    ${env:CXX}"
echo "GYP_MSVS_OVERRIDE_PATH: ${vcpath}\bin"

Start-Sleep -s 2

# configure
cmake -G "$generator" -DCMAKE_BUILD_TYPE="$config" -DSKIP_PACKAGING=ON "$arango_source" @Params

# build - with msbuild
if ($do_build) {
    cmake --build . --config "$config"
}
