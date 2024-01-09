# escape=`

FROM mcr.microsoft.com/windows/servercore:10.0.17763.4737-amd64

MAINTAINER hackers@arangodb.com

# Restore the default Windows shell for correct batch processing.
SHELL ["cmd", "/S", "/C"]

# use old chocolatey version to avoid dependency on .NET Framework 4.8 which does not work on this old base image
ENV chocolateyVersion=1.4.0
ENV CHOCO_URL=https://chocolatey.org/install.ps1
COPY install-openssl.ps1 C:\tools\

ENV COMPLUS_NGenProtectedProcess_FeatureEnabled=0

RUN `
    # Download the Build Tools bootstrapper.
    # We explicitly use the 17.3.6 release because newer versions are known to have issues with iresearch
    # All installer versions can be found here: https://learn.microsoft.com/en-us/visualstudio/releases/2022/release-history#fixed-version-bootstrappers
    curl -SL --output vs_buildtools.exe https://download.visualstudio.microsoft.com/download/pr/5c9aef4f-a79b-4b72-b379-14273860b285/bd2dd3a59d2553382f89712d19e4d5c3d930d9a41c9426cf8194dd5a3a75875f/vs_BuildTools.exe && `
    # Install Build Tools with the Microsoft.VisualStudio.Workload.AzureBuildTools workload, excluding workloads and components with known issues.
    (start /w vs_buildtools.exe --quiet --wait --norestart --nocache `
        --installPath "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools" `
        --add Microsoft.VisualStudio.Workload.VCTools `
        --add Microsoft.Component.MSBuild `
        --add Microsoft.VisualStudio.Component.VC.CoreBuildTools `
        --add Microsoft.VisualStudio.Component.VC.CMake.Project `
        --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        --add Microsoft.VisualStudio.Component.Windows11SDK.22621 `
        || IF "%ERRORLEVEL%"=="3010" EXIT 0) `
    `
    # Cleanup
    && del /q vs_buildtools.exe `
    # ngen .NET Fx
    && %windir%\Microsoft.NET\Framework64\v4.0.30319\ngen uninstall "Microsoft.Tpm.Commands, Version=10.0.0.0, Culture=Neutral, PublicKeyToken=31bf3856ad364e35, processorArchitecture=amd64" `
    && %windir%\Microsoft.NET\Framework64\v4.0.30319\ngen update `
    # clean out the package cache to reduce image size
    && @powershell Remove-Item 'C:/ProgramData/Package Cache/*' -Recurse -Force; `
    Set-ExecutionPolicy Bypass -Scope Process -Force; `
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]'Tls,Tls11,Tls12'; `
    iex ((New-Object System.Net.WebClient).DownloadString("$env:CHOCO_URL")); `
    choco install git winflexbison strawberryperl nasm python3 ccache -y
    
RUN @powershell git config --global --add safe.directory *; `
    $env:PATH += ';C:\Program Files\NASM'; `
    "C:/tools/install-openssl.ps1" -branch "3.1" -revision ".4"; `
    # clean out the temp folder to reduce image size
    Remove-Item -Path '$env:TEMP/*' -Recurse -Force -ErrorAction SilentlyContinue; `
    # perl and nasm are only needed to build OpenSSL
    choco uninstall strawberryperl nasm -y

ENV OPENSSL_ROOT_DIR=C:\openssl-3.1.4
ENV CCACHE_DIR=C:\ccache

# Define the entry point for the docker container.
# This entry point starts the developer command prompt and launches the PowerShell shell.
#ENTRYPOINT ["cmd.exe", "/S", "/C", "call \"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\Tools\\VsDevCmd.bat\"", "&&"]
#ENTRYPOINT ['echo "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\Tools\\VsDevCmd.bat"', "&&"]
#ENTRYPOINT ["cmd.exe", "/S", "/C"]
ENTRYPOINT ["CMD.EXE", "/C", "call", "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\Tools\\VsDevCmd.bat", "&&"]
