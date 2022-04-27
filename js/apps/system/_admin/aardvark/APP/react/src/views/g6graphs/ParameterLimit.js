import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterLimit = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [limit, setLimit] = useState(urlParameters.limit);

  const NEWURLPARAMETERS = { ...urlParameters };

  /*
  <h5>urlParameters (in ParameterLimit): {JSON.stringify(urlParameters)}</h5>
  */
  
  return (
    <>
      <form>
        <label>
          Limit:
          <input
            type="text"
            value={limit}
            onChange={(e) => {
              setLimit(+e.target.value);
              NEWURLPARAMETERS.limit = +e.target.value;
              setUrlParameters(NEWURLPARAMETERS);
            }}
          />
        </label>
      </form>
    </>
  );
};

export default ParameterLimit;
