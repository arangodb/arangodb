#REQUIRES -Version 2.0
<#
.SYNOPSIS
    Configures and builds ArangoDB
.EXAMPLE
    mkdir arango-build; cd arangod-build; ../arangodb/scripts/configure/<this_file>
#>

$config="RelWithDebInfo"
$script_path = split-path -parent $myinvocation.mycommand.definition
. "$script_path/vs2017-x64-common.ps1"
