import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterEdgeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColor, setEdgeColor] = useState(urlParameters.edgeColor);

  const NEWURLPARAMETERS = { ...urlParameters };
  let calculatedEdgeColor = edgeColor;
  if (!edgeColor.startsWith('#')) {
    calculatedEdgeColor = '#' + edgeColor;
  }

  return (
    <div>
      <Textinput
        label={'Color'}
        type={'color'}
        value={calculatedEdgeColor}
        width={'60px'}
        height={'30px'}
        onChange={(e) => {
          setEdgeColor(e.target.value);
          NEWURLPARAMETERS.edgeColor = e.target.value.replace("#", "");
          setUrlParameters(NEWURLPARAMETERS);
        }}
      disabled={urlParameters.edgeColorByCollection}>
      </Textinput>
      <Tooltip placement="bottom" title={"Default edge color."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterEdgeColor;
