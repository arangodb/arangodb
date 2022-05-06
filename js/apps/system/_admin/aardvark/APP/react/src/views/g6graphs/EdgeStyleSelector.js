import React, { useState } from 'react';
import { Select, Tooltip } from 'antd';
import PlainLabel from "./components/pure-css/form/PlainLabel";
import { InfoCircleFilled } from '@ant-design/icons';

const EdgeStyleSelector = ({ onEdgeStyleChange} ) => {
  const [type, setType] = useState('line');
  const SelectOption = Select.Option;
  const strokeColorInput = React.createRef();
  const lineWidthInput = React.createRef();

  const labelModel = {
      label: {
        fill: "#fff",
        fontSize: 12,
        background: {
          fill: "lightgreen",
          radius: 8,
          stroke: "#000"
        }
      }
  };

  const styles = [
    {
      type: 'line'
    },
    {
      type: 'arrow'
    },
    {
      type: 'curve'
    },
    {
      type: 'dotted'
    },
    {
      type: 'dashed'
    },
    {
      type: 'tapered'
    }
  ];

  const handleChange = type => {
    const typeModelMerged = {
      type: type
    };
    setType(type);
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
      <Tooltip placement="bottom" title={"The type of the edge."}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
    </div>
  );
}

export default EdgeStyleSelector;
