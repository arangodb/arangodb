/* global arangoHelper, arangoFetch, frontendConfig, document, $ */
import React, { useState, useEffect, useCallback } from 'react';
import styled from "styled-components";
import { Modal, Button } from 'antd';
import { JsonEditor as Editor } from 'jsoneditor-react';


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

  export const EditModal = ({ shouldShow, onUpdateNode, onRequestClose, node, nodeData, editorContent, children, nodeKey, nodeCollection }) => {

    const [visible, setVisibility] = useState(shouldShow);
    const [loading, setLoading] = useState(false);

    const updateNode = (graphData, updateNodeId) => {
      console.log("#################");
      console.log("EditModal - updateNode (node): ", node);
      console.log("#################");

      const model = {
        "_key": "Cologne",
        "_id": "germanCity/Cologne",
        "_rev": "_dlGzD4u--B",
        "population": 13000,
        "isCapital": false,
        "geometry": {
          "type": "Point",
          "coordinates": [
            6.9528,
            50.9364
          ]
        }
      };

      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/document/germanCity?returnNew=true'),
        data: JSON.stringify([model]),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          console.log("Successfully document saved: ", data);
        },
        error: function (data) {
          console.log("Error saving document: ", data);
        }
      });
      /*
      $.ajax({
        cache: false,
        type: 'PUT',
        url: arangoHelper.databaseUrl('/_api/document/' + encodeURIComponent(colid) + '?returnNew=true'),
        data: JSON.stringify([model]),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(false, data);
        },
        error: function (data) {
          callback(true, data);
        }
      });
      */

      onRequestClose();
      
      //onUpdateNode(mergedGraphData);
    }

    const okClicked = () => {
      setVisibility(false);
      console.log("OK clicked in ANTD modal");
    }

    const cancelClicked = () => {
      setVisibility(false);
      console.log("CANCEL clicked in ANTD modal");
    }

  return shouldShow ? (
      <ModalBackground onClick={onRequestClose}>
        <ModalBody onClick={(e) => e.stopPropagation()}>
          <div>
            {children}<br />
          </div>
          <div>
            {
              nodeData ? (
                <Editor value={nodeData} onChange={() => console.log('Data in jsoneditor changed')} mode={'code'} history={true} />
              ) : 'Data is loading...'
            }
          </div>
          <div>
            <button onClick={onRequestClose}>Cancel</button>
            <button className="button-success" onClick={() => { updateNode(node) }}>Update</button>
          </div>
        </ModalBody>
      </ModalBackground>
  ) : null;
 /*
  return (
  <Modal
  visible={visible}
  title="Title"
  onOk={() => okClicked()}
  onCancel={() => cancelClicked()}
  footer={[
    <Button key="back" onClick={() => cancelClicked()}>
      Return
    </Button>,
    <Button key="submit" type="primary" loading={loading} onClick={() => okClicked()}>
      Submit
    </Button>,
    <Button
      key="link"
      href="https://google.com"
      type="primary"
      loading={loading}
      onClick={() => okClicked()}
    >
      Search on Google
    </Button>,
  ]}
>
  <p>Some contents...</p>
  <p>Some contents...</p>
  <p>Some contents...</p>
  <p>Some contents...</p>
  <p>Some contents...</p>
</Modal>
  )
  */
};
