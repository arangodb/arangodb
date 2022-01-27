/* global arangoHelper, arangoFetch, frontendConfig, document, $ */
import React, { useState, useEffect, useCallback } from 'react';
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

export const LoadDocument = ({ node }) => {

  const [nodeData, setNodeData] = useState([]);

  console.log("<<<<< Load document >>>>>");
  console.log("<<<<< Load document (node): ", node);

  useEffect(() => {
    arangoFetch(arangoHelper.databaseUrl("/_api/simple/lookup-by-keys"), {
      method: "PUT",
      body: JSON.stringify(node),
    })
    .then(response => response.json())
    .then(data => {
      console.log("Document loaded by LoadDocument component: ", data);
      setNodeData(data);
    })
    .catch((err) => {
      console.log(err);
    });
  }, [node]);

  if(nodeData) {
    return <strong>Node data loaded!</strong>
  }
  return <strong>No node data loaded</strong>;

  /*
  if(node) {
    const slashPos = node.id.indexOf("/");
    const label = node.id.substring(slashPos + 1) + " - " + node.id.substring(0, slashPos);
  }
  */

  /*
    // fetching document # start
    const fetchDoc = useCallback(() => {
      const data = {
        "keys": [
          "Berlin"
        ],
        "collection": "germanCity"
      };
    
      arangoFetch(arangoHelper.databaseUrl("/_api/simple/lookup-by-keys"), {
        method: "PUT",
        body: JSON.stringify(data),
      })
      .then(response => response.json())
      .then(data => {
        console.log("Document loaded by LoadDocument component: ", data);;
        //setSelectedNodeData(data);
      })
      .catch((err) => {
        console.log(err);
      });
    }, []);

    useEffect(() => {
      fetchDoc();
    }, [fetchDoc]);
    // fetching document # end
    */

  /*
  return data ? (
      <ModalBackground onClick={onRequestClose}>
        <ModalBody onClick={(e) => e.stopPropagation()}>
          <div>
            {children}
          </div>
          <button onClick={onRequestClose}>Cancel</button>
          <button className="button-success" onClick={() => { updateNode(node) }}>Update</button>
        </ModalBody>
      </ModalBackground>
  ) : null;
  */
};
