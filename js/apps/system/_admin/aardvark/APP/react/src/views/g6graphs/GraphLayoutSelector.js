import React, { useState } from 'react';
import { Select } from 'antd';

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
      layout: 'force'
    },
    {
      layout: 'fruchterman'
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
              <label for="graphlayout">Layout</label>
              <Select name="graphlayout" style={{ width: '200px' }} value={layout} onChange={handleChange}>
                {layouts.map(style => {
                  const { layout } = style;
                  return (
                    <SelectOption key={layout} value={layout}>
                      {layout}
                    </SelectOption>
                  );
                })}
              </Select>
            </div>
          </form>
  </>;
}

export default GraphLayoutSelector;
