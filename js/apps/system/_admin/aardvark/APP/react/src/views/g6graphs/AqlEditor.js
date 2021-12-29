import React, { useState } from 'react';

const AqlEditor = ({
  queryString, onQueryChange, onNewSearch, onOnclickHandler, onAqlQueryHandler
}) => {
  const clearData = {
    limit: '',
    depth: ''
  };
  let [formData, setFormData] = useState(clearData);
  let textInput = React.createRef();
  let nodesInput = React.createRef();

  return (
    <>
      <input
        type="text"
        onChange={(event) => { setFormData({ ...formData, limit: event.target.value})}}
        value={formData.limit}
        placeholder="Limit"
        style={{width: '90%'}} /><br />
      <input
        type="text"
        onChange={(event) => { setFormData({ ...formData, depth: event.target.value})}}
        value={formData.depth}
        placeholder="depth"
        style={{width: '90%'}} /><br />
      <button
        className="button-primary"
        onClick={
          () => {
            const myString = `/_admin/aardvark/graph/routeplanner?nodeLabelByCollection=false&nodeColorByCollection=true&nodeSizeByEdges=true&edgeLabelByCollection=false&edgeColorByCollection=false&nodeStart=frenchCity/Paris&depth=${formData.depth}&limit=${formData.limit}&nodeLabel=_key&nodeColor=#2ecc71&nodeColorAttribute=&nodeSize=&edgeLabel=&edgeColor=#cccccc&edgeColorAttribute=&edgeEditable=true`;
            onAqlQueryHandler(myString)}
        }>
          Load AQL Query
      </button>
    </>
  );
  /*
  return (
          <div className="d-flex align-items-center bg-light px-3 py-2 small rounded-3">
              <div className="d-flex align-items-center flex-grow-1">
                <label htmlFor="queryString" className="me-2 fw-bold text-secondary">
                  Select graph data:
                </label>
                <input
                  ref={textInput}
                  id="queryString"
                  className="form-control form-control-sm me-2"
                  type="text"
                />
              </div>
              <div className="d-flex align-items-center">
                <button
                  className="btn mx-1 btn-sm btn-primary bi bi-search"
                  onClick={()=> onOnclickHandler(textInput.current.value)}>
                    Receive data
                </button>
              </div>
          </div>
      );
      */
  };

export default AqlEditor;
