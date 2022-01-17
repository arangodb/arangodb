import React, { useState } from 'react';
import { Select } from 'antd';

const NodeStyleSelector = ({ onNodeStyleChange} ) => {
  const [type, setType] = useState('circle');
  const SelectOption = Select.Option;
  const fillColorInput = React.createRef();
  const strokeColorInput = React.createRef();
  const widthInput = React.createRef();
  const heightInput = React.createRef();

  const typeModel = {
    labelCfg: {
      position: 'bottom',
      offset: 10,
      style: {
        fontSize: 24,
        fill: '#404a53',
        fontWeight: 700
      },
    },
  };

  const styles = [
    {
      type: 'circle'
    },
    {
      type: 'rect',
    },
    {
      type: 'ellipse',
    },
    {
      type: 'diamond',
    },
    {
      type: 'triangle',
    },
    {
      type: 'star',
    },
    {
      type: 'image',
      img: 'https://gw.alipayobjects.com/zos/rmsportal/XuVpGqBFxXplzvLjJBZB.svg',
    },
    {
      type: 'modelRect',
    }
  ];

  const handleChange = type => {
    const typeModelMerged = {
      ...typeModel,
      type: type,
      size: [
        (widthInput.current.value ? widthInput.current.value : 20),
        (heightInput.current.value ? heightInput.current.value : 20)],
      style: {
        fill: fillColorInput.current.value,
        stroke: strokeColorInput.current.value,
      },
    };
    setType(type);
    onNodeStyleChange(typeModelMerged);
  };

  return <>
          <h4>Node style</h4>
          <label for="fillcolor">Fill color</label>
          <input name="fillcolor" type="color" style={{ width: '200px', height: '20px' }} ref={fillColorInput} />
          <label for="strokecolor">Stroke color</label>
          <input name="strokecolor" type="color" style={{ width: '200px', height: '20px' }} ref={strokeColorInput} />
          <label for="width">Width</label>
          <input name="width" type="input" style={{ width: '200px' }} placeholder="20" ref={widthInput} />
          <label for="height">Height</label>
          <input name="height" type="input" style={{ width: '200px' }} placeholder="20" ref={heightInput} />
          <label for="nodetype">Type</label>
          <Select name="nodeType" style={{ width: '200px' }} value={type} onChange={handleChange}>
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

export default NodeStyleSelector;
