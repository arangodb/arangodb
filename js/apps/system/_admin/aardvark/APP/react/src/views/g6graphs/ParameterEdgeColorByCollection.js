import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterEdgeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorByCollection, setEdgeColorByCollection] = useState(urlParameters.edgeColorByCollection);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <>
      <label>
        <input
          type="checkbox"
          checked={edgeColorByCollection}
          onChange={() => {
            const newEdgeColorByCollection = !edgeColorByCollection;
            setEdgeColorByCollection(newEdgeColorByCollection);
            NEWURLPARAMETERS.edgeColorByCollection = newEdgeColorByCollection;
            setUrlParameters(NEWURLPARAMETERS);
          }}
        />
        Color edges by collection
      </label>
      <p>Do we color edges by their collection? {edgeColorByCollection.toString()}</p>
      <Tooltip title="Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored.">
        <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
      </Tooltip>
    </>
  );
};

export default ParameterEdgeColorByCollection;
