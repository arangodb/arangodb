import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Switch, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterEdgeLabelByCollection = () => {
  const urlParameters = useContext(UrlParametersContext);

  const onEdgeLabelByCollectionChange = (checked) => {
    urlParameters.edgeLabelByCollection = checked;
  }
  
  return (
    <>
      <Switch
        checkedChildren="Show collection name"
        unCheckedChildren="Hide collection name"
        onChange={onEdgeLabelByCollectionChange}
        style={{ marginTop: '24px' }}
      />
      <Tooltip title="Set label text by collection. If activated edge label attribute will be ignored.">
        <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
      </Tooltip>
    </>
  );
};

export default ParameterEdgeLabelByCollection;
