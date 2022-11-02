# Conveniently run clang-format.sh in Git Bash from PowerShell
#
# pre-requisite: Windows-based system
# pre-requisite: PowerShell 3+
# pre-requisite: Docker
# pre-requisite: Git for Windows 2.9+ including Git Bash 4+

$adb_path = Split-Path $PSScriptRoot -Parent
Push-Location $adb_path

$git_for_windows = Get-ItemProperty -ErrorAction Stop 'Registry::HKEY_LOCAL_MACHINE\SOFTWARE\GitForWindows'

if (![bool]$git_for_windows.InstallPath) {
    throw 'Could not find Git InstallPath in registry'
}

# Using bash.exe because git-bash.exe would open a new window
$git_bash = Join-Path ($git_for_windows).InstallPath 'bin' 'bash.exe'
$files = ''
if ($args.length -gt 0) {
    $files = $args.Replace('\', '/')
}
& $git_bash './scripts/clang-format.sh' $files

Pop-Location
