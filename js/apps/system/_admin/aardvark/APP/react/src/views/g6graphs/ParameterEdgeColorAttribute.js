import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterEdgeColorAttribute = ({ edgesColorAttributes }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorAttribute, setEdgeColorAttribute] = useState(urlParameters.edgeColorAttribute);

  const newUrlParameters = { ...urlParameters };
  
  return (
    <div>
      <Textinput
        label={'Edge color attribute'}
        value={edgeColorAttribute}
        width={'300px'}
        onChange={(e) => {
          setEdgeColorAttribute(e.target.value);
          newUrlParameters.edgeColorAttribute = e.target.value;
          setUrlParameters(newUrlParameters);
        }}
        disabled={urlParameters.edgeColorByCollection}>
      </Textinput>
      <Tooltip placement="bottom" title={"If an attribute is given, edges will then be colorized by the attribute. This setting ignores default edge color if set."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
};

export default ParameterEdgeColorAttribute;
