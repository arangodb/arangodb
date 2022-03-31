/* global arangoHelper, arangoFetch */

import React, { useEffect, useState, useCallback, useRef } from 'react';

const SetStartnodeComponent = ({ onSetStartnode }) => {
  const graphname = window.localStorage.getItem('graphname');
  const clearData = {
    startnode: ''
  };
  let [formData, setFormData] = useState(clearData);

  /*
  http://localhost:8529/_db/_system/_admin/aardvark/graph/routeplanner?nodeLabelByCollection=false&nodeColorByCollection=true&nodeSizeByEdges=true&edgeLabelByCollection=false&edgeColorByCollection=false&nodeStart=germanCity/Cologne&depth=2&limit=250&nodeLabel=_key&nodeColor=#2ecc71&nodeColorAttribute=&nodeSize=&edgeLabel=&edgeColor=#cccccc&edgeColorAttribute=&edgeEditable=true
  */

  const setNewStartnode = () => {
    arangoFetch(arangoHelper.databaseUrl(`/_admin/aardvark/graph/${graphname}?nodeLabelByCollection=false&nodeColorByCollection=true&nodeSizeByEdges=true&edgeLabelByCollection=false&edgeColorByCollection=false&nodeStart=${formData.startnode}&depth=2&limit=250&nodeLabel=_key&nodeColor=#2ecc71&nodeColorAttribute=&nodeSize=&edgeLabel=&edgeColor=#cccccc&edgeColorAttribute=&edgeEditable=true`), {
      method: "GET"
    })
    .then(response => response.json())
    .then(data => {
      console.log("setNewStartnode (data): ", data);
      onSetStartnode(data);
      setFormData(clearData);
    })
    .catch((err) => {
      console.log(err);
    });
  }

  return <>
    <label for="setstartnode">Startnode</label>
    <input
      id="setstartnode"
      name="setstartnode" 
      type="text"
      onChange={(event) => { setFormData({ ...formData, startnode: event.target.value})}}
      value={formData.startnode}
      placeholder="startnode id"
    />
    <button onClick={() => setNewStartnode()}></button>
  </>;
};

export default SetStartnodeComponent;
