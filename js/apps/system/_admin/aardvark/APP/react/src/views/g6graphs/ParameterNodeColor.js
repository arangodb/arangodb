import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColor, setNodeColor] = useState(urlParameters.nodeColor);

  const NEWURLPARAMETERS = { ...urlParameters };
  let calculatedNodeColor = nodeColor;
  if (!nodeColor.startsWith('#')) {
    calculatedNodeColor = '#' + nodeColor;
  }

  return (
    <div>
      <Textinput
        label={'Color'}
        type={'color'}
        value={calculatedNodeColor}
        width={'60px'}
        height={'30px'}
        onChange={(e) => {
          setNodeColor(e.target.value);
          NEWURLPARAMETERS.nodeColor = e.target.value.replace("#", "");
          setUrlParameters(NEWURLPARAMETERS);
      }}
      disabled={urlParameters.nodeColorByCollection}>
      </Textinput>
      <Tooltip placement="bottom" title={"Default node color."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterNodeColor;
