import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Switch, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeLabelByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabelByCollection, setNodeLabelByCollection] = useState(urlParameters.nodeLabelByCollection);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <>
      <label>
        <input
          type="checkbox"
          checked={nodeLabelByCollection}
          onChange={() => {
            const newNodeLabelByCollection = !nodeLabelByCollection;
            setNodeLabelByCollection(newNodeLabelByCollection);
            NEWURLPARAMETERS.nodeLabelByCollection = newNodeLabelByCollection;
            setUrlParameters(NEWURLPARAMETERS);
          }}
        />
        Show collection name
      </label>
      <p>Do we show the collection name? {nodeLabelByCollection.toString()}</p>
      <Tooltip title="Append collection name to the label?">
        <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
      </Tooltip>
    </>
  );
};

export default ParameterNodeLabelByCollection;
