import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeStart = () => {
  const urlParameters = useContext(UrlParametersContext);

  const onNodeStartChange = (e) => {
    urlParameters.nodeStart = e.target.value;
  }
  
  return (
    <>
      <Input
        addonBefore="Startnode"
        placeholder={urlParameters.nodeStart || ''}
        onChange={onNodeStartChange}
        suffix={
          <Tooltip title="A valid node id or space seperated list of id's. If empty a random node will be chosen.">
            <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
          </Tooltip>
        }
        style={{ width: 240, marginTop: '24px' }}
      />
    </>
  );
};

export default ParameterNodeStart;
