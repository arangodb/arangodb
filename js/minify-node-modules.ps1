New-Item -Type Directory node_modules-bundled

foreach ($dir in $(Get-ChildItem node\node_modules -Directory)) {
  if ($dir.Name -eq "mocha") {
    Copy-Item -Recurse node\node_modules\$dir node_modules-bundled\
    continue
  }
  $start=(Get-Content node\node_modules\$dir\package.json | ConvertFrom-Json).main
  if ($start -eq $null) {
    $start = "index.js"
  }
  if (!$start.EndsWith(".js")) {
    $start += ".js"
  }
  New-Item -Type Directory node_modules-bundled\$dir
  Set-Content -Path node_modules-bundled\$dir\package.json "{}"
  webpack.cmd node\node_modules\$dir\$start node_modules-bundled\$dir\index.js --target node --output-library-target commonjs2
}