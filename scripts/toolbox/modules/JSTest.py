#!/usr/bin/env python

import subprocess
import os
from contextlib import contextmanager


'''
This is a small wrapper around or arangosh jsunity test runner
It basically starts an arangosh and runs the following script:

const res = require('jsunity').runTest('${fileName}', true);
const proc = require('@arangodb/testutils/result-processing');
proc.analyze.saveToJunitXML({testXmlOutputDirectory: '${workDir}'}, {transform: {poc: res}});


Where fileName and workDir are the input parameters generated in the Python code below.
'''

def start(options, cfg):
    folderName = options["folderName"] + "/tests"
    # for fileName in os.listdir(folderName):
    ## dirty hack to only run one test
    fileName = folderName + "/test.js"
    workDir = os.getcwd() + "/" + cfg["globals"]["workDir"] + "/junit"

    os.makedirs(workDir, exist_ok=True)

    parameters = [cfg["arangosh"]["executable"]]

    for key, value in cfg["arangosh"]["startupParameters"].items():
        parameters.append("--" + key + "=" + value)

    textStr = f'const res = require("jsunity").runTest("{fileName}", true);'
    textStr += f'require("@arangodb/testutils/result-processing")'
    textStr += f'.analyze.saveToJunitXML({{testXmlOutputDirectory: "{workDir}"}}, {{transform: {{poc: res}}}});'

    parameters.append('--javascript.execute-string=' + textStr)
    process = subprocess.Popen(parameters, stdout=subprocess.DEVNULL)
    process.wait()
    return process
