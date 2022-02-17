import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeLabel = () => {
  const urlParameters = useContext(UrlParametersContext);

  const onNodeLabelChange = (e) => {
    urlParameters.nodeLabel = e.target.value;
  }
  
  return (
    <>
      <Input
              addonBefore="Search limit"
              placeholder={urlParameters.nodeLabel || ''}
              onChange={onNodeLabelChange}
              suffix={
                <Tooltip title="Node label. Please choose a valid and available node attribute.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
    </>
  );
};

export default ParameterNodeLabel;
