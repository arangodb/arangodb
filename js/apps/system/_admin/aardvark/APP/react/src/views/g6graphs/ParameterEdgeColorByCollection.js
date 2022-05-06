import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Checkbox, Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';

const ParameterEdgeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorByCollection, setEdgeColorByCollection] = useState(urlParameters.edgeColorByCollection);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <div>
      <Checkbox
        checked={edgeColorByCollection}
        onChange={() => {
          const newEdgeColorByCollection = !edgeColorByCollection;
          setEdgeColorByCollection(newEdgeColorByCollection);
          NEWURLPARAMETERS.edgeColorByCollection = newEdgeColorByCollection;
          setUrlParameters(NEWURLPARAMETERS);
        }}>
        Color edges by collection
      </Checkbox>
      <Tooltip placement="bottom" title={"Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
      <p>Do we color edges by their collection? {edgeColorByCollection.toString()}</p>
    </div>
  );
};

export default ParameterEdgeColorByCollection;
