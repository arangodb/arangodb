import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterDepth = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [depth, setDepth] = useState(urlParameters.depth);

  const NEWURLPARAMETERS = { ...urlParameters };

  return (
    <>
      <form>
        <Textinput
            label={'Depth'}
            value={depth}
            width={'60px'}
            onChange={(e) => {
              setDepth(+e.target.value);
              NEWURLPARAMETERS.depth = +e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
            }}>
        </Textinput>
        <Tooltip placement="bottom" title={"Search depth, starting from your start node."}>
          <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
        </Tooltip>
      </form>
    </>
  );
};

export default ParameterDepth;
