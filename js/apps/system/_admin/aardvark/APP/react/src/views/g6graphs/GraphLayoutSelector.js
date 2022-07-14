import React, { useState } from 'react';
import { Select, Tooltip } from 'antd';
import PlainLabel from "./components/pure-css/form/PlainLabel";
import { InfoCircleFilled } from '@ant-design/icons';

const GraphLayoutSelector = ({ onGraphLayoutChange} ) => {
  const [layout, setLayout] = useState('gForce');
  const SelectOption = Select.Option;

  const layouts = [
    {
      layout: 'random'
    },
    {
      layout: 'gForce'
    },
    {
      layout: 'circular'
    },
    {
      layout: 'radial'
    },
    {
      layout: 'mds'
    },
    {
      layout: 'dagre'
    },
    {
      layout: 'concentric'
    },
    {
      layout: 'comboForce'
    },
    {
      layout: 'comboCombined'
    }
  ];

  const handleChange = layout => {
    setLayout(layout);
    onGraphLayoutChange(layout);
  };

  return <>
    <form>
      <div>
        <PlainLabel htmlFor={'graphlayout'}>Layout</PlainLabel>
        <Select
          name="graphlayout"
          value={layout}
          className="graphReactViewContainer"
          dropdownClassName="graphReactViewContainer"
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
          {layouts.map(style => {
            const { layout } = style;
            return (
              <SelectOption 
                key={layout} 
                value={layout} 
                className='graphReactViewContainer' 
                dropdownClassName="graphReactViewContainer"
              >
                {layout}
              </SelectOption>
            );
          })}
        </Select>
        <Tooltip placement="bottom" title={"Graph layouts are the algorithms arranging the node positions."}>
          <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
        </Tooltip>
      </div>
    </form>
  </>;
}

export default GraphLayoutSelector;
