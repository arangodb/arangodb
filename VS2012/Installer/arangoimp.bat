@echo off
:: #################################################################
:: # script to start the ArangoDB shell in Windows
:: #################################################################
setlocal enableextensions

arangosh.exe -c arangosh.conf %*