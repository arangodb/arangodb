import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Checkbox, Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';

const ParameterNodeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorByCollection, setNodeColorByCollection] = useState(urlParameters.nodeColorByCollection);

  const newUrlParameters = { ...urlParameters };

  return (
    <div>
      <Checkbox
        checked={nodeColorByCollection}
        onChange={() => {
          const newNodeColorByCollection = !nodeColorByCollection;
          setNodeColorByCollection(newNodeColorByCollection);
          newUrlParameters.nodeColorByCollection = newNodeColorByCollection;
          setUrlParameters(newUrlParameters);
        }}
        style={{ 'color': '#736b68' }}
      >
        Color nodes by collection
      </Checkbox>
      <Tooltip placement="bottom" title={"Should nodes be colorized by their collection? If enabled, node color and node color attribute will be ignored."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterNodeColorByCollection;
