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

  export const AddNodeModal2 = ({ shouldShow, onUpdateNode, onRequestClose, node, nodeData, editorContent, children, nodeKey, nodeCollection, onNodeCreation }) => {

    const [visible, setVisibility] = useState(shouldShow);
    const [nodeKeyName, setNodeKeyName] = useState('');
    const [loading, setLoading] = useState(false);
    const jsonEditorRef = useRef();
    const [json, setJson] = useState(nodeData);
    const [isModalVisible, setIsModalVisible] = useState(shouldShow);

    const openNotificationWithIcon = type => {
      notification[type]({
        message: 'Node created',
        description:
          `The node ${nodeKeyName} was successfully created`,
      });
    };

    const addNode = (graphData, updateNodeId) => {
      /*
      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/document/' + nodeCollection + '?returnNew=true'),
        data: JSON.stringify([json]),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          openNotificationWithIcon('success');
          console.log("Successfully document saved: ", data);
        },
        error: function (data) {
          console.log("Error saving document: ", data);
        }
      });
      */
      var url = arangoHelper.databaseUrl('/_api/document?collection=' + encodeURIComponent('germanCity'));

      const newNodeData = {
        "_key": "Hannover",
        "population": 1,
        "isCapital": false,
        "geometry": {
          "type": "Point",
          "coordinates": [
            6.9528,
            51.9364
          ]
        }
      };

      $.ajax({
        cache: false,
        type: 'POST',
        url: url,
        data: JSON.stringify([json]),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          setNodeKeyName('Duesseldorf');
          openNotificationWithIcon('success');
          onNodeCreation(data);
          console.log("Successfully node created: ", data);
        },
        error: function (data) {
          console.log("Error creating node: ", data);
        }
      });
      onRequestClose();
      
      //onUpdateNode(mergedGraphData);
    }

    const showModal = () => {
      setIsModalVisible(true);
    };

    const handleOk = () => {
      setIsModalVisible(false);
    };

    const handleCancel = () => {
      setIsModalVisible(false);
    };

    /*
  return (
    <>
      <Modal title="Basic Modal" visible={isModalVisible} onOk={handleOk} onCancel={handleCancel}>
      <div>
            {children}<br />
          </div>
          <div>
            {
              nodeData ? (
                <Editor
                  ref={jsonEditorRef}
                  value={nodeData}
                  onChange={(value) => {
                    console.log('Data in jsoneditor changed: ', value);
                    setJson(value);
                  }}
                  mode={'code'}
                  history={true} />
              ) : 'Data is loading...'
            }
          </div>
      </Modal>
    </> );
    */

  return shouldShow ? (
      <ModalBackground onClick={onRequestClose}>
        <ModalBody onClick={(e) => e.stopPropagation()}>
          <div>
            {children}<br />
          </div>
          <div>
            {
              nodeData ? (
                <Editor
                  ref={jsonEditorRef}
                  value={nodeData}
                  onChange={(value) => {
                    console.log('Data in jsoneditor changed: ', value);
                    setJson(value);
                  }}
                  mode={'code'}
                  history={true} />
              ) : 'Data is loading...'
            }
          </div>
          <div style={{ 'margin-top': '38px', 'text-align': 'right' }}>
            <button className="button-close" onClick={onRequestClose}>Cancel</button>
            <button className="button-success" onClick={() => { addNode(node) }}>Create</button>
          </div>
        </ModalBody>
      </ModalBackground>
  ) : null;
};
