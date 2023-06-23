#!/usr/bin/env python

import json

import modules.ArangoDBInstanceManager as ArangoDBInstanceManager
import modules.Feed as Feed
import modules.ArgumentParser as ArgumentParser

def executePipeline(cfg, args, fileName):
    if fileName is None:
        raise Exception("No pipeline file given.")

    with open(fileName, 'r') as f:
        pipeline = json.load(f)
        print("Executing pipeline:")
        print(pipeline)

        for element in pipeline:
            parameters = None
            if "parameters" in element:
                parameters = element["parameters"]

            # Start Cluster Component
            if element["component"] == "startCluster":
                ArangoDBInstanceManager.startCluster()

            # Stop Cluster Component
            elif element["component"] == "stopCluster":
                ArangoDBInstanceManager.stopCluster()

            # Feed Data Insertion Component
            elif element["component"] == "feed":
                assert ("folderName" in parameters)
                folderName = parameters["folderName"]
                Feed.startFromFile(cfg, folderName)

