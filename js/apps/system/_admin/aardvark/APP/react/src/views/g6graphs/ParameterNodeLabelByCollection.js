import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Switch, Tooltip } from 'antd';
import { InfoCircleOutlined } from '@ant-design/icons';

const ParameterNodeLabelByCollection = () => {
  const urlParameters = useContext(UrlParametersContext);

  const onNodeLabelByCollectionChange = (checked) => {
    urlParameters.nodeLabelByCollection = checked;
  }
  
  return (
    <>
      <Switch
        checkedChildren="Show collection name"
        unCheckedChildren="Hide collection name"
        onChange={onNodeLabelByCollectionChange}
        style={{ marginTop: '24px' }}
      />
      <Tooltip title="Append collection name to the label?">
        <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
      </Tooltip>
    </>
  );
};

export default ParameterNodeLabelByCollection;
