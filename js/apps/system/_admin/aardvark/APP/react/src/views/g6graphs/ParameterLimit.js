import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Tooltip } from 'antd';
import Textinput from "./components/pure-css/form/Textinput.tsx";
import { InfoCircleFilled } from '@ant-design/icons';

const ParameterLimit = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [limit, setLimit] = useState(urlParameters.limit);

  const newUrlParameters = { ...urlParameters };

  return (
    <>
      <form>
        <Textinput
          label={'Limit'}
          value={limit}
          width={'60px'}
          onChange={(e) => {
            setLimit(+e.target.value);
            newUrlParameters.limit = +e.target.value;
            setUrlParameters(newUrlParameters);
          }}>
        </Textinput>
        <Tooltip placement="bottom" title={"Limit nodes count. If empty or zero, no limit is set."}>
          <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
        </Tooltip>
      </form>
    </>
  );
};

export default ParameterLimit;
