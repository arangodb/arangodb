import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";


const ParameterNodeColorAttribute = ({ nodesColorAttributes }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorAttribute, setNodeColorAttribute] = useState(urlParameters.nodeColorAttribute);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <>
      <form>
        <Textinput
          label={'Node color attribute'}
          value={nodeColorAttribute}
          width={'300px'}
          onChange={(e) => {
            setNodeColorAttribute(e.target.value);
            NEWURLPARAMETERS.nodeColorAttribute = e.target.value;
            setUrlParameters(NEWURLPARAMETERS);
          }}
          disabled={urlParameters.nodeColorByCollection}>
        </Textinput>
        <Tooltip placement="bottom" title={"If an attribute is given, nodes will then be colorized by the attribute. This setting ignores default node color if set."}>
          <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
        </Tooltip>
      </form>
    </>
  );
};

export default ParameterNodeColorAttribute;
