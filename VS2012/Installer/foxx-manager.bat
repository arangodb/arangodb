@echo off
:: #################################################################
:: # script to start the foxx manager in Windows
:: #################################################################
setlocal enableextensions

arangosh.exe -c arangosh.conf --javascript.execute-string "require('org/arangodb/foxx/manager').run(ARGUMENTS);" %*
