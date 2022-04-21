import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorByCollection, setNodeColorByCollection] = useState(urlParameters.nodeColorByCollection);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <>
      <label>
        <input
          type="checkbox"
          checked={nodeColorByCollection}
          onChange={() => {
            const newNodeColorByCollection = !nodeColorByCollection;
            setNodeColorByCollection(newNodeColorByCollection);
            NEWURLPARAMETERS.nodeColorByCollection = newNodeColorByCollection;
            setUrlParameters(NEWURLPARAMETERS);
          }}
        />
        Color nodes by collection
      </label>
      <p>Do we color nodes by their collection? {nodeColorByCollection.toString()}</p>
      <Tooltip title="Should nodes be colorized by their collection? If enabled, node color and node color attribute will be ignored.">
        <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
      </Tooltip>
    </>
  );
};

export default ParameterNodeColorByCollection;
