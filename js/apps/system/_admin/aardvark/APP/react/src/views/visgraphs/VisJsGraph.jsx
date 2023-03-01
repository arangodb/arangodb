/* global arangoHelper, arangoFetch, $ */

import React, { useCallback, useEffect, useState } from 'react';
import { GraphView } from './GraphView';
import { data } from './data';
import { UrlParametersContext } from "./url-parameters-context";
import URLPARAMETERS from "./UrlParameters";

const VisJsGraph = () => {
  const currentUrl = window.location.href;
  const [graphName, setGraphName] = useState(currentUrl.substring(currentUrl.lastIndexOf("/") + 1));
  let localUrlParameters = URLPARAMETERS;
  const settingsKey = `${graphName}-gv-urlparameters`;
  if(localStorage.getItem(settingsKey) !== null) {
    localUrlParameters = JSON.parse(localStorage.getItem(settingsKey));
  }
  const [urlParameters, setUrlParameters] = React.useState(localUrlParameters);

  let responseTimesObject = {
    fetchStarted: null,
    fetchFinished: null,
    fetchDuration: null
  }

  let urlParamsObject = {
    depth: 1,
    limit: 1,
    nodeColorByCollection: true,
    edgeColorByCollection: false,
    nodeColor: "#2ecc71",
    nodeColorAttribute: '',
    edgeColor: '#cccccc',
    edgeColorAttribute: '',
    nodeLabel: 'key',
    edgeLabel: '',
    nodeSize: '',
    nodeSizeByEdges: true,
    edgeEditable: true,
    nodeLabelByCollection: false,
    edgeLabelByCollection: false,
    nodeStart: '',
    barnesHutOptimize: true,
    query: '',
    mode: 'all'
  };

  const [responseTimes, setResponseTimes] = useState(responseTimesObject);
  const [visQueryString, setVisQueryString] = useState(`/_admin/aardvark/visgraph/${graphName}`);
  const [visQueryMethod, setVisQueryMethod] = useState("GET");
  const [graphData, setGraphData] = useState(data);
  const [visGraphData, setVisGraphData] = useState(data);

  const fetchVisData = useCallback(() => {
    responseTimesObject.fetchStarted = new Date();
    $.ajax({
      type: visQueryMethod,
      url: arangoHelper.databaseUrl(visQueryString),
      contentType: 'application/json',
      data: urlParameters,
      success: function (data) {
        responseTimesObject.fetchFinished = new Date();
        responseTimesObject.fetchDuration = Math.abs(responseTimesObject.fetchFinished.getTime() - responseTimesObject.fetchStarted.getTime());
        setResponseTimes(responseTimesObject);
        setVisGraphData(data);
      },
      error: function (e) {
        arangoHelper.arangoError('Graph', e.responseJSON.errorMessage);
        console.log(e);
      }
    });
  }, [visQueryString]);

  useEffect(() => {
    fetchVisData();
  }, [fetchVisData]);

  return (
    <div>
      <UrlParametersContext.Provider value={[urlParameters, setUrlParameters]}>
        <GraphView
          visGraphData={visGraphData}
          graphName={graphName}
          responseDuration={responseTimes.fetchDuration}
          onChangeGraphData={(newGraphData) => setVisGraphData(newGraphData)}
          onGraphDataLoaded={({newGraphData, responseTimesObject}) => {
            setVisGraphData(newGraphData);
            setResponseTimes(responseTimesObject);
            setGraphData(newGraphData);
          }}
        />
      </UrlParametersContext.Provider>
    </div>
  );
}

export default VisJsGraph;
