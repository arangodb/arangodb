import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterEdgeLabel = () => {
  const urlParameters = useContext(UrlParametersContext);

  const onEdgeLabelChange = (e) => {
    urlParameters.edgeLabel = e.target.value;
  }
  
  return (
    <>
      <Input
              addonBefore="Label"
              placeholder={urlParameters.edgeLabel || ''}
              onChange={onEdgeLabelChange}
              suffix={
                <Tooltip title="Default edge label">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
    </>
  );
};

export default ParameterEdgeLabel;
