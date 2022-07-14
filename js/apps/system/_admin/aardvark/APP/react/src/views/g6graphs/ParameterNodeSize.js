import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeSize = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSize, setNodeSize] = useState(urlParameters.nodeSize);

  const newUrlParameters = { ...urlParameters };

  return (
    <div style={{ marginBottom: '20px' }}>
      <Textinput
        label={'Sizing attribute'}
        value={nodeSize}
        width={'300px'}
        onChange={(e) => {
          setNodeSize(e.target.value);
          newUrlParameters.nodeSize = e.target.value;
          setUrlParameters(newUrlParameters);
        }}
        disabled={urlParameters.nodeSizeByEdges}>
      </Textinput>
      <Tooltip placement="bottom" title={"If an attribute is given, nodes will be sized by the attribute."} overlayClassName='graph-border-box'>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterNodeSize;
