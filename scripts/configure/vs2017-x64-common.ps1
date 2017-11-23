#REQUIRES -Version 2.0
<#
.SYNOPSIS
    Configures and builds ArangoDB
.EXAMPLE
    mkdir arango-build; cd arangod-build; ../arangodb/scripts/configure/<this_file>
#>
$ErrorActionPreference = "Stop"
$PSDefaultParameterValues['*:ErrorAction']='Stop'

$arango_source = split-path -parent (split-path -parent $script_path)

$vcpath=$(Get-ItemProperty HKLM:\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VC7)."14.0"
$env:GYP_MSVS_OVERRIDE_PATH="${vcpath}\bin"
$env:CC="${vcpath}\bin\cl.exe"
$env:CXX="${vcpath}\bin\cl.exe"

echo "Configuration:          $config"
echo "Source Path:            ${arango_source}"
echo "CC:                     ${env:CC}"
echo "CXX:                    ${env:CXX}"
echo "GYP_MSVS_OVERRIDE_PATH: ${vcpath}\bin"

Start-Sleep -s 4

#Set-PSDebug -Trace 2
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE="$config" -DSKIP_PACKAGING=ON "$arango_source"
cmake --build . --config "$config"
