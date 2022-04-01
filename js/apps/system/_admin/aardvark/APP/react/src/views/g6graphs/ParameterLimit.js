import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterLimit = () => {
  const urlParameters = useContext(UrlParametersContext);

  const onLimitChange = (e) => {
    urlParameters.limit = e.target.value;
  }
  
  return (
    <>
      <Input
              addonBefore="Search limit"
              placeholder={urlParameters.limit || ''}
              onChange={onLimitChange}
              suffix={
                <Tooltip title="Limit nodes count. If empty or zero, no limit is set.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px', marginBottom: '24px' }}
            />
    </>
  );
};

export default ParameterLimit;
