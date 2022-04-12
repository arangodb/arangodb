import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Switch, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterEdgeLabelByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabelByCollection, setEdgeLabelByCollection] = useState(urlParameters.edgeLabelByCollection);

  const NEWURLPARAMETERS = { ...urlParameters };
  
  return (
    <>
      <label>
        <input
          type="checkbox"
          checked={edgeLabelByCollection}
          onChange={() => {
            const newEdgeLabelByCollection = !edgeLabelByCollection;
            console.log("newEdgeLabelByCollection: ", newEdgeLabelByCollection);
            setEdgeLabelByCollection(newEdgeLabelByCollection);
            NEWURLPARAMETERS.edgeLabelByCollection = newEdgeLabelByCollection;
            setUrlParameters(NEWURLPARAMETERS);
          }}
        />
        Show collection name
      </label>
      <p>Do we show the collection name? {edgeLabelByCollection.toString()}</p>
      <Tooltip title="Append collection name to the label?">
        <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
      </Tooltip>
    </>
  );
};

export default ParameterEdgeLabelByCollection;
