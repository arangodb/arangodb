import React, { useState } from 'react';
import { Select } from 'antd';

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

  return <>
          <div>
            <label for="edgetype">Type</label>
            <Select name="edgeType" style={{ width: '200px' }} value={type} onChange={handleChange}>
              {styles.map(style => {
                const { type } = style;
                return (
                  <SelectOption key={type} value={type}>
                    {type}
                  </SelectOption>
                );
              })}
            </Select>
          </div>
  </>;
}

export default EdgeStyleSelector;
