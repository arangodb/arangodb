import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Input, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeColorAttribute = () => {
  const urlParameters = useContext(UrlParametersContext);

  const onNodeColorAttributeChange = (e) => {
    urlParameters.nodeColorAttribute = e.target.value;
  }
  
  return (
    <>
      <Input
        addonBefore="Color attribute"
        placeholder={urlParameters.nodeColorAttribute || ''}
        onChange={onNodeColorAttributeChange}
        suffix={
          <Tooltip title="If an attribute is given, nodes will then be colorized by the attribute. This setting ignores default node color if set.">
            <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
          </Tooltip>
        }
        style={{ width: 240, marginTop: '24px' }}
      />
    </>
  );
};

export default ParameterNodeColorAttribute;
