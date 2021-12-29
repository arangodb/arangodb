/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useState, useEffect, useCallback } from 'react';

const CollectionLoader = ({ onLoadCollection }) => {
  let [queryString, setQueryString] = useState("/_api/gharial");
  const clearData = {
    graphName: ''
  };
  let [formData, setFormData] = useState(clearData);
  const [collections, setCollections] = useState([]);
  let textInput = React.createRef();
  let nodesInput = React.createRef();

  // fetching data # start
  const fetchData = useCallback(() => {
    arangoFetch(arangoHelper.databaseUrl(queryString), {
      method: "GET",
    })
    .then(response => response.json())
    .then(data => {
      console.log("CollectionLoader (data): ", data);
      console.log("CollectionLoader (data.graphs): ", data.graphs);
      setCollections(data.graphs);
    })
    .catch((err) => {
      console.log(err);
    });
  }, [queryString]);

  useEffect(() => {
    fetchData();
  }, [fetchData]);
  // fetching data # end

  return (
    <>
      <div className='graph-list'>
        <input
          onChange={(event) => { setFormData({ ...formData, graphName: event.target.value})}}
          value={formData.graphName}
          list="graphlist"
          id="graphcollections"
          name="graphcollections"
          placeholder="Choose graph..."
          style={{width: '90%'}}
        />
        <datalist id="graphlist">
          {collections.map(collection => (
            <option key={collection._key} value={collection._key} />
          ))}
        </datalist>
      </div>
      <button
        className="button-primary"
        onClick={
          () => {
            onLoadCollection(formData.graphName);
            setFormData(clearData);}
        }
        style={{"marginLeft": "0px" }}>
          Load graph
      </button>
    </>
  );
};

export default CollectionLoader;
