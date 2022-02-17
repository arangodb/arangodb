import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterDepth = () => {
  const urlParameters = useContext(UrlParametersContext);

  const onDepthChange = (e) => {
    urlParameters.depth = e.target.value;
  }
  
  return (
    <>
      <Input
              addonBefore="Search depth"
              placeholder={urlParameters.depth || ''}
              onChange={onDepthChange}
              suffix={
                <Tooltip title="Search depth, starting from your startnode">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
    </>
  );
};

export default ParameterDepth;
