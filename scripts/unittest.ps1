function Get-Build-Path {
  for ($idx = 0; $idx -lt $args.count; $idx++) {
    if ($args -eq "--build") {
      return $args[$idx + 1]
    }
  }
}

$EXT = ".exe"
$EXEC_PATH = Split-Path -Path $PSScriptRoot
$BUILD_DIR_PARAM = Get-Build-Path @args
$env:PORT = 1024 + $(Get-Random -Maximum 1024)

if ($null -eq $env:ARANGOSH) {
  if ($null -ne $BUILD_DIR_PARAM -And $(Test-Path -Path "$BUILD_DIR_PARAM/bin/arangosh$EXT" -PathType Leaf)) {
    $ARANGOSH = "$BUILD_DIR_PARAM/bin/arangosh$EXT"
  } elseif (Test-Path -Path "build/bin/RelWithDebInfo/arangosh$EXT" -PathType Leaf) {
    $ARANGOSH = "build/bin/RelWithDebInfo/arangosh$EXT"
  } elseif (Test-Path -Path "build/bin/arangosh$EXT" -PathType Leaf) {
    $ARANGOSH = "build/bin/arangosh$EXT"
  } elseif (Test-Path -Path "bin/arangosh$EXT" -PathType Leaf) {
    $ARANGOSH = "bin/arangosh$EXT"
  } elseif (Test-Path -Path "arangosh$EXT" -PathType Leaf) {
    $ARANGOSH = "arangosh$EXT"
  } else {
    $ARANGOSH = Get-ChildItem -Path $EXEC_PATH -Name arangosh.exe -Recurse
    if ($null -eq $ARANGOSH) {
      Write-Error "Cannot locate arangosh.exe!"
      Exit 1
    }
    if ($ARANGOSH -is [array]) {
      $ARANGOSH = $ARANGOSH[0]
    }
    Write-Warning "WARNING: Using guessed arangosh location $ARANGOSH"
  }
} else {
  $ARANGOSH = $env:ARANGOSH
}

$arguments = @(
  "-c etc/relative/arangosh.conf",
  "--log.level warning",
  "--server.endpoint none",
  "--javascript.execute UnitTests/unittest.js"
)

Start-Process `
  -FilePath "$ARANGOSH" `
  -WorkingDirectory "$EXEC_PATH" `
  -ArgumentList "$arguments -- $args" `
  -NoNewWindow `
  -Wait
