/* global arangoHelper, $ */
import React from 'react';
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

  & div, & button {
    box-sizing: content-box !important;
  }
  
`;

const ModalBody = styled.div`
  background-color: white;
  margin: 10% auto;
  padding: 20px;
  width: 50%;
`;

const StyledButton = styled.button`
  margin-left: 15px !important;
  color: white !important;
`;

export const FetchFullGraphModal = ({ shouldShow, onRequestClose, children, onFullGraphLoaded, graphName }) => {

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
      "nodeSizeByEdges": "true",
      "edgeEditable": "true",
      "nodeLabelByCollection": "false",
      "edgeLabelByCollection": "false",
      "nodeStart": "",
      "barnesHutOptimize": true,
      "mode": "all"
    };

    $.ajax({
      type: 'GET',
      url: arangoHelper.databaseUrl(`/_admin/aardvark/graph/${graphName}`),
      contentType: 'application/json',
      data: ajaxData,
      success: function (data) {
        onRequestClose();
        onFullGraphLoaded(data);
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
          <StyledButton className="button-close" onClick={onRequestClose}>Cancel</StyledButton>
          <StyledButton className="button-success" onClick={() => { fetchFullGraph() }}>Load full graph</StyledButton>
        </div>
      </ModalBody>
    </ModalBackground>
  ) : null;
};
