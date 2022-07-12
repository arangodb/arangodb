/* global arangoHelper, $ */
import React, { useState } from 'react';
import styled from "styled-components";

const ModalBackground = styled.div`
  position: fixed;
  z-index: 1;
  left: 0;
  top: 0;
  width: 100%;
  height: 100%;
  overflow: auto;
  background-color: rgba(0, 0, 0, 0.5);
`;

const ModalBody = styled.div`
  background-color: white;
  margin: 10% auto;
  padding: 20px;
  width: 50%;
`;

export const FetchFullGraphModal = ({ shouldShow, onRequestClose, children, onFullGraphLoaded, graphName }) => {
let responseTimesObject = {
    fetchStarted: null,
    fetchFinished: null,
    fetchDuration: null
  }
  const [responseTimes, setResponseTimes] = useState(responseTimesObject);

  const fetchFullGraph = () => {
    
    const ajaxData = {
      "depth": "2",
      "limit": "250",
      "nodeColor": "#CBDF2F",
      "nodeColorAttribute": "",
      "nodeColorByCollection": "false",
      "edgeColor": "#cccccc",
      "edgeColorAttribute": "",
      "edgeColorByCollection": "false",
      "nodeLabel": "_key",
      "edgeLabel": "",
      "nodeSize": "",
      "nodeSizeByEdges": "false",
      "edgeEditable": "true",
      "nodeLabelByCollection": "false",
      "edgeLabelByCollection": "false",
      "nodeStart": "",
      "barnesHutOptimize": true,
      "mode": "all"
    };
    responseTimesObject.fetchStarted = new Date();

    $.ajax({
      type: 'GET',
      url: arangoHelper.databaseUrl(`/_admin/aardvark/graph/${graphName}`),
      contentType: 'application/json',
      data: ajaxData,
      success: function (data) {
        responseTimesObject.fetchFinished = new Date();
        responseTimesObject.fetchDuration = Math.abs(responseTimesObject.fetchFinished.getTime() - responseTimesObject.fetchStarted.getTime());
        setResponseTimes(responseTimesObject);
        onRequestClose();
        onFullGraphLoaded(data, responseTimesObject);
        const element = document.getElementById("graph-card");
        element.scrollIntoView({ behavior: "smooth" });
      },
      error: function (e) {
        arangoHelper.arangoError('Graph', 'Could not update this edge.');
      }
    });
  }

  return shouldShow ? (
    <ModalBackground onClick={onRequestClose}>
      <ModalBody onClick={(e) => e.stopPropagation()}>
        <div>
          {children}<br />
        </div>
        <div style={{ 'margin-top': '38px', 'text-align': 'right' }}>
          <button className="button-close" onClick={onRequestClose}>Cancel</button>
          <button className="button-success" onClick={() => { fetchFullGraph() }}>Load full graph</button>
        </div>
      </ModalBody>
    </ModalBackground>
  ) : null;
};
