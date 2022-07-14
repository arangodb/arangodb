import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";


const ParameterNodeColorAttribute = ({ nodesColorAttributes }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorAttribute, setNodeColorAttribute] = useState(urlParameters.nodeColorAttribute);

  const newUrlParameters = { ...urlParameters };

  return (
    <>
      <div>
        <Textinput
          label={'Node color attribute'}
          value={nodeColorAttribute}
          width={'300px'}
          onChange={(e) => {
            setNodeColorAttribute(e.target.value);
            newUrlParameters.nodeColorAttribute = e.target.value;
            setUrlParameters(newUrlParameters);
          }}
          disabled={urlParameters.nodeColorByCollection}>
        </Textinput>
        <Tooltip placement="bottom" title={"If an attribute is given, nodes will be colorized by the attribute. This setting ignores default node color if set."}>
          <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
        </Tooltip>
      </div>
    </>
  );
};

export default ParameterNodeColorAttribute;
