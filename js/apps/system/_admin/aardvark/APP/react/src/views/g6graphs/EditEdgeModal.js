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

  export const EditEdgeModal = ({ shouldShow, onUpdateEdge, onRequestClose, edge, edgeData, basicEdgeData, editorContent, children, edgeKey, edgeCollection }) => {

    const [visible, setVisibility] = useState(shouldShow);
    const [loading, setLoading] = useState(false);
    const jsonEditorRef = useRef();
    const [json, setJson] = useState(edgeData);
    const [isModalVisible, setIsModalVisible] = useState(shouldShow);

    const openNotificationWithIcon = type => {
      notification[type]({
        message: 'Edge updates',
        description:
          `The edge ${edge} was successfully updated`,
      });
    };

    const updateEdge = (graphData, updateEdgeId) => {
      console.log("basicEdgeData: ", basicEdgeData);
      console.log("json: ", json);
      const newEdgeData = {
        ...basicEdgeData,
        ...json
      };
      console.log("newEdgeData: ", newEdgeData);
      
      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/document/' + edgeCollection + '?returnNew=true'),
        //data: JSON.stringify([json]),
        data: JSON.stringify([newEdgeData]),
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
              edgeData ? (
                <Editor
                  ref={jsonEditorRef}
                  value={edgeData}
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
              edgeData ? (
                <Editor
                  ref={jsonEditorRef}
                  value={edgeData}
                  onChange={(value) => {
                    console.log('Data in jsoneditor changed (EditEdgeModal): ', value);
                    setJson(value);
                  }}
                  mode={'code'}
                  history={true} />
              ) : 'Data is loading...'
            }
          </div>
          <div style={{ 'margin-top': '38px', 'text-align': 'right' }}>
            <button className="button-close" onClick={onRequestClose}>Cancel</button>
            <button className="button-success" onClick={() => { updateEdge(edge) }}>Update</button>
          </div>
        </ModalBody>
      </ModalBackground>
  ) : null;
};
