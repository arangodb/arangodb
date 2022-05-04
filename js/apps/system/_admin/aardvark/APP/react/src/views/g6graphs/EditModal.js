/* global arangoHelper, arangoFetch, frontendConfig, document, $ */
import React, { useState, useEffect, useCallback, useRef } from 'react';
import styled from "styled-components";
import { JsonEditor as Editor } from 'jsoneditor-react';
import { Modal, Button, notification, Space } from 'antd';
import { omit } from "lodash";


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

  export const EditModal = ({ shouldShow, onUpdateNode, onRequestClose, node, nodeData, basicNodeData, editorContent, children, nodeKey, nodeCollection }) => {

    const [visible, setVisibility] = useState(shouldShow);
    const [loading, setLoading] = useState(false);
    const jsonEditorRef = useRef();
    const jsonEditorRef2 = useRef();
    /*
    const [json, setJson] = useState(omit(nodeData, '_id', '_key', '_rev'));
    */
    const [json, setJson] = useState(nodeData);
    const [isModalVisible, setIsModalVisible] = useState(shouldShow);

    const openNotificationWithIcon = type => {
      notification[type]({
        message: 'Node updates',
        description:
          `The node ${node} was successfully updated`,
      });
    };

    const updateNode = (graphData, updateNodeId) => {
          console.log("basicNodeData: ", basicNodeData);
          console.log("json: ", json);
          const newNodeData = {
              ...basicNodeData,
              ...json
          };
          console.log("newNodeData: ", newNodeData);
          $.ajax({
            cache: false,
            type: 'PUT',
            url: arangoHelper.databaseUrl('/_api/document/' + nodeCollection + '?returnNew=true'),
            //data: JSON.stringify([json]),
            data: JSON.stringify([newNodeData]),
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
                    console.log('Data in jsoneditor changed (EditModal): ', value);
                    setJson(value);
                  }}
                  mode={'code'}
                  history={true} />
              ) : 'Loading data...'
            }
          </div>
          <div style={{ 'margin-top': '38px', 'text-align': 'right' }}>
            <button className="button-close" onClick={onRequestClose}>Cancel</button>
            <button className="button-success" onClick={() => { updateNode(node) }}>Update</button>
          </div>
        </ModalBody>
      </ModalBackground>
  ) : null;
};
