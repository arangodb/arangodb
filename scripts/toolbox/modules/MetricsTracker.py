#!/usr/bin/env python
import threading
import time
from prometheus_client.parser import text_string_to_metric_families
from threading import Thread
import requests
import os
import json
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
from prometheus_client.samples import Sample
import warnings


class MetricsTracker:
    def __init__(self, cfg, metricsNames=None):
        if metricsNames is None:
            self.metricsNames = []
        else:
            self.metricsNames = metricsNames

        self.daemon = None
        self.interval = cfg["metrics"]["interval"]
        self.endpoint = cfg["metrics"]["endpoint"]
        self.workDir = cfg["globals"]["workDir"]
        self.stopEvent = threading.Event()
        self.store = []  # used to store the metrics
        self.timestamps = []  # used to store the timestamps

    def addSpecificMetric(self, metric):
        self.metricsNames.append(metric)

    def addSpecificMetrics(self, metrics):
        self.metricsNames.extend(metrics)

    def filterMetric(self, actualMetricName):
        print(self.metricsNames)
        if len(self.metricsNames) > 0:
            for filterName in self.metricsNames:
                if filterName in actualMetricName:
                    return False
        else:
            return True

        return True

    def getMetric(self, store):
        timestamp = time.time()
        self.timestamps.append(timestamp)
        try:
            metrics = requests.get(self.endpoint).text
            for family in text_string_to_metric_families(metrics):
                for sample in family.samples:
                    # Check Filters
                    if self.filterMetric(sample.name):
                        continue

                    timedSample = Sample(sample.name, sample.labels, sample.value, timestamp)
                    store.append(timedSample)
        except Exception as e:
            print(e)

    def watchDog(self, store, stopEvent):
        while not stopEvent.is_set():
            self.getMetric(store)
            time.sleep(self.interval)

    def start(self):
        self.daemon = Thread(target=self.watchDog, daemon=True, name='WatchDog', args=(self.store, self.stopEvent))
        self.daemon.start()

    def stop(self):
        self.stopEvent.set()

    def metricsToGraph(self, dictData, fileName):
        for metric in dictData:
            if "bucket" in metric:
                warnings.warn("Metric: " + metric + " is a bucket metric and will not be plotted.")
                continue
            else:
                pngFileName = fileName + "-" + metric + ".png"
                plotValues = []
                for value in dictData[metric]["values"]:
                    plotValues.append(value[1])  # value without timestamp

                # Metrics to file as plot data
                fig, ax = plt.subplots()
                t = self.timestamps
                s = np.array(plotValues)
                fmt = ticker.FuncFormatter(lambda x, pos: time.strftime('%H:%M:%S', time.gmtime(x)))
                ax.xaxis.set_major_formatter(fmt)
                ax.plot(t, s)
                ax.set(xlabel='time (s)', ylabel='probably bytes (check docu)',
                       title=metric)
                ax.grid()
                fig.savefig(pngFileName)
                plt.close()

    def metricsToFile(self, jsonFile, fileName):
        convertedDict = {}

        for sample in self.store:
            if sample.name in convertedDict:
                convertedDict[sample.name]["values"].append(
                    [sample.timestamp, sample.value]
                )
            else:
                convertedDict[sample.name] = {
                    "values": [[sample.timestamp, sample.value, ]]
                }

        # Metrics to file as json
        jsonFile.write(json.dumps(convertedDict))
        self.metricsToGraph(convertedDict, fileName)

    def write(self, fileName):
        timeString = time.strftime("%Y%m%d-%H%M%S")
        jsonFileName = self.workDir + "/metrics/" + fileName + "-" + timeString + ".json"
        pngDirName = self.workDir + "/metrics/plots/"
        pngPartialFileName = pngDirName + fileName + "-" + timeString
        os.makedirs(os.path.dirname(jsonFileName), exist_ok=True)
        os.makedirs(os.path.dirname(pngPartialFileName), exist_ok=True)
        with open(jsonFileName, "w") as file:
            self.metricsToFile(file, pngPartialFileName)

    def stopAndWrite(self, fileName):
        self.stop()
        self.write(fileName)
