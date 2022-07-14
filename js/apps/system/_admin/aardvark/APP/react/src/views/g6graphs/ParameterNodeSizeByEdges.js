import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Checkbox, Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';

const ParameterNodeSizeByEdges = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSizeByEdges, setNodeSizeByEdges] = useState(urlParameters.nodeSizeByEdges);

  const newUrlParameters = { ...urlParameters };

  return (
    <div>
      <Checkbox
        checked={nodeSizeByEdges}
        onChange={() => {
          const newNodeSizeByEdges = !nodeSizeByEdges;
          setNodeSizeByEdges(newNodeSizeByEdges);
          newUrlParameters.nodeSizeByEdges = newNodeSizeByEdges;
          setUrlParameters(newUrlParameters);
        }}
        style={{ 'color': '#736b68' }}
      >
        Size by connections
      </Checkbox>
      <Tooltip placement="bottom" title={"If enabled, nodes are sized according to the number of edges they have. This option overrides the sizing attribute."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterNodeSizeByEdges;
