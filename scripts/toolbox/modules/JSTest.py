#!/usr/bin/env python

import subprocess
import os
from contextlib import contextmanager


@contextmanager
def cwd(path):
    oldPwd = os.getcwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(oldPwd)

def start(options, cfg):
    folderName = options["folderName"] + "/tests"
    dummyTestFile = cfg["globals"]["workDir"] + "/schmutz.js"
    # for fileName in os.listdir(folderName):
    ## dirty hack to only run one test
    fileName = folderName + "/test.js"
    workDir = os.getcwd() + "/" + cfg["globals"]["workDir"] + "/junit"

    with open(dummyTestFile, "w") as textFile:
        textFile.write(f'''const res = require('jsunity').runTest('{fileName}', true);''')
        textFile.write(f'''require('@arangodb/testutils/result-processing')''')
        textFile.write(f'''.analyze.saveToJunitXML({{testXmlOutputDirectory: '{workDir}'}}, {{transform: {{poc: res}}}});''')

    parameters = [cfg["arangosh"]["executable"]]

    for key, value in cfg["arangosh"]["startupParameters"].items():
        parameters.append("--" + key + "=" + value)

    parameters.append("--javascript.execute=" + dummyTestFile)
    os.makedirs(workDir, exist_ok=True)
    process = subprocess.Popen(parameters, stdout=subprocess.DEVNULL)
    process.wait()
    return process
