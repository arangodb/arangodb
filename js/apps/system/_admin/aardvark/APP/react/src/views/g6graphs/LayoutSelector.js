import React, { useState } from "react";
import { Select } from "antd";

const SelectOption = Select.Option;
const LayoutSelector = ({props, onLayoutChange}) => {
  const [layout, setLayout] = useState('graphin-force');

  const onChange = (value) => {
    console.log("new value for layout: ", value);
    onLayoutChange(value);
    setLayout(value);
    console.log("layout value in LayoutSelector: ", layout);
  }

  const layouts = [
    {
      type: 'graphin-force'
    },
    {
        type: 'grid',
    },
    {
        type: 'circular',
    },
    {
        type: 'radial',
    },
    {
        type: 'force',
        preventOverlap: true,
        linkDistance: 50,
        nodeStrength: 30,
        edgeStrength: 0.8,
        collideStrength: 0.8,
        nodeSize: 30,
        alpha: 0.9,
        alphaDecay: 0.3,
        alphaMin: 0.01,
        forceSimulation: null,
        onTick: () => {
            console.log('ticking');
        },
        onLayoutEnd: () => {
            console.log('force layout done');
        },
    },
    {
        type: 'gForce',
        linkDistance: 150,
        nodeStrength: 30,
        edgeStrength: 0.1,
        nodeSize: 30,
        onTick: () => {
            console.log('ticking');
        },
        onLayoutEnd: () => {
            console.log('force layout done');
        },
        workerEnabled: false,
        gpuEnabled: false,
    },
    {
        type: 'concentric',
        maxLevelDiff: 0.5,
        sortBy: 'degree',
    },
    {
        type: 'dagre',
        rankdir: 'LR',
    },
    {
        type: 'fruchterman',
    },
    {
        type: 'mds',
        workerEnabled: false,
    },
    {
        type: 'comboForce',
    },
  ];
  //const { value, onChange } = props;
  //const { layouts } = apis.getInfo();
  /*
  {layouts.map(item => {
          const { name, disabled, desc } = item;
          return (
            <SelectOption key={name} value={name} disabled={disabled}>
              {desc}
            </SelectOption>
          );
        })}
        <div style={{ position: "absolute", top: 62, left: 10 }}>
        */
  return (
    <div>
      <Select style={{ marginTop: '24px', width: '240px' }} value={layout} onChange={onChange}>
        <SelectOption key='graphin-force' value='graphin-force'>
        </SelectOption>
        <SelectOption key='grid' value='grid'>
        </SelectOption>
        <SelectOption key='circular' value='circular'>
        </SelectOption>
        <SelectOption key='radial' value='radial'>
        </SelectOption>
        <SelectOption key='force' value='force'>
        </SelectOption>
        <SelectOption key='gForce' value='gForce'>
        </SelectOption>
        <SelectOption key='concentric' value='concentric'>
        </SelectOption>
        <SelectOption key='dagre' value='dagre'>
        </SelectOption>
        <SelectOption key='fruchterman' value='fruchterman'>
        </SelectOption>
        <SelectOption key='mds' value='mds'>
        </SelectOption>
        <SelectOption key='comboForce' value='comboForce'>
        </SelectOption>
      </Select>
    </div>
  );
};
export default LayoutSelector;
