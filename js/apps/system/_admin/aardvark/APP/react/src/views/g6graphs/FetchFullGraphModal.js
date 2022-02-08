/* global arangoHelper, arangoFetch, frontendConfig, document, $ */
import React, { useState, useEffect, useCallback, useRef } from 'react';
import styled from "styled-components";
import { JsonEditor as Editor } from 'jsoneditor-react';
import { Modal, Button, notification, Space } from 'antd';


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

    const [isModalVisible, setIsModalVisible] = useState(shouldShow);

    const showModal = () => {
      setIsModalVisible(true);
    };

    const handleOk = () => {
      setIsModalVisible(false);
    };

    const handleCancel = () => {
      setIsModalVisible(false);
    };

    const fetchFullGraph = () => {
      const ajaxData = {
        "depth": "2",
        "limit": "250",
        "nodeColor": "#2ecc71",
        "nodeColorAttribute": "",
        "nodeColorByCollection": "true",
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
          console.log("Full graph loaded: ", data);
          onRequestClose();
          onFullGraphLoaded(data);
        },
        error: function (e) {
          arangoHelper.arangoError('Graph', 'Could not load full graph.');
        }
      });
      /*
      $.ajax({
        cache: false,
        type: 'GET',
        //url: arangoHelper.databaseUrl('/_api/document/' + nodeCollection + '?returnNew=true'),
        url: url,
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          //openNotificationWithIcon('success');
          console.log("Full graph loaded: ", data);
        },
        error: function (data) {
          console.log("Error loading full graph: ", data);
        }
      });
      */
      //onRequestClose();
      
      //onUpdateNode(mergedGraphData);
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
