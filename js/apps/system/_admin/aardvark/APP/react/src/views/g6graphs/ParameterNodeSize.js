import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeSize = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeSize, setNodeSize] = useState(urlParameters.nodeSize);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <div style={{ marginBottom: '20px' }}>
      <Textinput
        label={'Sizing attribute'}
        value={nodeSize}
        width={'300px'}
        onChange={(e) => {
          setNodeSize(e.target.value);
          NEWURLPARAMETERS.nodeSize = e.target.value;
          setUrlParameters(NEWURLPARAMETERS);
        }}
        disabled={urlParameters.nodeSizeByEdges}>
      </Textinput>
      <Tooltip placement="bottom" title={"Default node size. Numeric value > 0."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterNodeSize;
