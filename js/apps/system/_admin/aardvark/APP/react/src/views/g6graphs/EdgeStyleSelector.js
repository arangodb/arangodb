import React, { useContext, useState } from 'react';
import { Select, Tooltip } from 'antd';
import PlainLabel from "./components/pure-css/form/PlainLabel";
import { InfoCircleFilled } from '@ant-design/icons';
import { UrlParametersContext } from "./url-parameters-context";

const EdgeStyleSelector = ({ onEdgeStyleChange} ) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [type, setType] = useState(urlParameters.edgeType);

  const newUrlParameters = { ...urlParameters };

  const SelectOption = Select.Option;

  const styles = [
    {
      type: 'solid'
    },
    {
      type: 'dashed'
    },
    {
      type: 'dotted'
    }
  ];

  const handleChange = type => {
    const typeModelMerged = {
      type: type
    };
    setType(type);
    newUrlParameters.edgeType = type;
    setUrlParameters(newUrlParameters);
    onEdgeStyleChange(typeModelMerged);
  };

  return (
    <div style={{ marginBottom: '20px' }}>
      <PlainLabel htmlFor={'edgeType'}>Type</PlainLabel>
      <Select
        name="edgeType"
        value={type}
        onChange={handleChange}
        style={{
          'width': '200px',
          'height': 'auto',
          'margin-right': '8px',
          'color': '#555555',
          'border': '2px solid rgba(140, 138, 137, 0.25)',
          'border-radius': '4px',
          'background-color': '#fff !important',
          'box-shadow': 'none',
          'outline': 'none',
          'outline-color': 'transparent',
          'outline-style': 'none'
        }}
      >
        {styles.map(style => {
          const { type } = style;
          return (
            <SelectOption key={type} value={type}>
              {type}
            </SelectOption>
          );
        })}
      </Select>
      <Tooltip placement="bottom" title={"The type of the edge."} overlayClassName="graph-border-box">
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
}

export default EdgeStyleSelector;
