import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Checkbox, Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';

const ParameterEdgeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorByCollection, setEdgeColorByCollection] = useState(urlParameters.edgeColorByCollection);

  const newUrlParameters = { ...urlParameters };

  return (
    <div>
      <Checkbox
        checked={edgeColorByCollection}
        onChange={() => {
          const newEdgeColorByCollection = !edgeColorByCollection;
          setEdgeColorByCollection(newEdgeColorByCollection);
          newUrlParameters.edgeColorByCollection = newEdgeColorByCollection;
          setUrlParameters(newUrlParameters);
        }}
        style={{ 'color': '#736b68' }}
      >
        Color edges by collection
      </Checkbox>
      <Tooltip placement="bottom" title={"Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterEdgeColorByCollection;
