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
      type: 'polyline',
    },
    {
      type: 'arc',
    },
    {
      type: 'quadratic',
    },
    {
      type: 'cubic',
    },
    {
      type: 'cubic-vertical',
    },
    {
      type: 'cubic-horizontal',
    },
    {
      type: 'loop',
    }
  ];

  const handleChange = type => {
    const typeModelMerged = {
      type: type,
      style: {
        ...labelModel,
        stroke: strokeColorInput.current.value,
        lineWidth: (lineWidthInput.current.value ? lineWidthInput.current.value : 2),
        endArrow: true,
      },
    };
    setType(type);
    onEdgeStyleChange(typeModelMerged);
  };

  return <>
          <h4>Edge style</h4>
          <label for="strokecolor">Stroke color</label>
          <input name="strokecolor" type="color" style={{ width: '200px', height: '20px' }} ref={strokeColorInput} />
          <label for="linewidth">Line width</label>
          <input name="linewidth" type="input" style={{ width: '200px' }} placeholder="5" ref={lineWidthInput} />
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
  </>;
}

export default EdgeStyleSelector;
