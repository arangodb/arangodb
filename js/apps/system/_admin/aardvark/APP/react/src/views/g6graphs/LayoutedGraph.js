import React from 'react';
import Graphin, { Utils } from '@antv/graphin';
import {Card, Select } from 'antd';
import {
  TrademarkCircleFilled,
  ChromeFilled,
  BranchesOutlined,
  ApartmentOutlined,
  AppstoreFilled,
  CopyrightCircleFilled,
  CustomerServiceFilled,
  ShareAltOutlined

} from '@ant-design/icons';

const LayoutedGraph = () => {
  const layouteddata = Utils.mock(10)
        .tree()
        .graphin();
  const iconMap = {
    'graphin-force': <ShareAltOutlined />,
    random: <TrademarkCircleFilled />,
    concentric: <ChromeFilled />,
    circle: <BranchesOutlined />,
    force: <AppstoreFilled />,
    dagre: <ApartmentOutlined />,
    grid: <CopyrightCircleFilled />,
    radial: <ShareAltOutlined />,
  };

    const SelectOption = Select.Option;
    const LayoutSelector = props => {
        const { value, onChange, options } = props;

        return (
            <div>
                <Select style={{ width: '120px' }} value={value} onChange={onChange}>
                    {options.map(item => {
                        const { type } = item;
                        const iconComponent = iconMap[type] || <CustomerServiceFilled />;
                        return (
                            <SelectOption key={type} value={type}>
                                {iconComponent} &nbsp;
                                {type}
                            </SelectOption>
                        );
                    })}
                </Select>
            </div>
        );
    };

    const layouts = [
        { type: 'graphin-force' },
        {
            type: 'grid',
            // begin: [0, 0], // optional，
            // preventOverlap: true, // optional
            // preventOverlapPdding: 20, // optional
            // nodeSize: 30, // optional
            // condense: false, // optional
            // rows: 5, // optional
            // cols: 5, // optional
            // sortBy: 'degree', // optional
            // workerEnabled: false, // optional
        },
        {
            type: 'circular',
            // center: [200, 200], // optional
            // radius: null, // optional
            // startRadius: 10, // optional
            // endRadius: 100, // optional
            // clockwise: false, // optional
            // divisions: 5, // optional
            // ordering: 'degree', // optional
            // angleRatio: 1, // optional
        },
        {
            type: 'radial',
            // center: [200, 200], // optional
            // linkDistance: 50, // optional，
            // maxIteration: 1000, // optional
            // focusNode: 'node11', // optional
            // unitRadius: 100, // optional
            // preventOverlap: true, // optional
            // nodeSize: 30, // optional
            // strictRadial: false, // optional
            // workerEnabled: false, // optional
        },
        {
            type: 'force',
            preventOverlap: true,
            // center: [200, 200], // optional
            linkDistance: 50, // optional
            nodeStrength: 30, // optional
            edgeStrength: 0.8, // optional
            collideStrength: 0.8, // optional
            nodeSize: 30, // optional
            alpha: 0.9, // optional
            alphaDecay: 0.3, // optional
            alphaMin: 0.01, // optional
            forceSimulation: null, // optional
            onTick: () => {
                // optional
                console.log('ticking');
            },
            onLayoutEnd: () => {
                // optional
                console.log('force layout done');
            },
        },
        {
            type: 'gForce',
            linkDistance: 150, // optional
            nodeStrength: 30, // optional
            edgeStrength: 0.1, // optional
            nodeSize: 30, // optional
            onTick: () => {
                // optional
                console.log('ticking');
            },
            onLayoutEnd: () => {
                // optional
                console.log('force layout done');
            },
            workerEnabled: false, // optional
            gpuEnabled: false, // optional
        },
        {
            type: 'concentric',
            maxLevelDiff: 0.5,
            sortBy: 'degree',
            // center: [200, 200], // optional

            // linkDistance: 50, // optional
            // preventOverlap: true, // optional
            // nodeSize: 30, // optional
            // sweep: 10, // optional
            // equidistant: false, // optional
            // startAngle: 0, // optional
            // clockwise: false, // optional
            // maxLevelDiff: 10, // optional
            // sortBy: 'degree', // optional
            // workerEnabled: false, // optional
        },
        {
            type: 'dagre',
            rankdir: 'LR', // optional
            // align: 'DL', // optional
            // nodesep: 20, // optional
            // ranksep: 50, // optional
            // controlPoints: true, // optional
        },
        {
            type: 'fruchterman',
            // center: [200, 200], // optional
            // gravity: 20, // optional
            // speed: 2, // optional
            // clustering: true, // optional
            // clusterGravity: 30, // optional
            // maxIteration: 2000, // optional
            // workerEnabled: false, // optional
            // gpuEnabled: false, // optional
        },
        {
            type: 'mds',
            workerEnabled: false, // optional
        },
        {
            type: 'comboForce',
            // // center: [200, 200], // optional
            // linkDistance: 50, // optional
            // nodeStrength: 30, // optional
            // edgeStrength: 0.1, // optional
            // onTick: () => {
            //   // optional
            //   console.log('ticking');
            // },
            // onLayoutEnd: () => {
            //   // optional
            //   console.log('combo force layout done');
            // },
        },
    ];

    const [type, setLayout] = React.useState('graphin-force');
    const handleChange = value => {
        console.log('value', value);
        setLayout(value);
    };

    const layout = layouts.find(item => item.type === type);
    return (
        <div>
                    <Card
                        title="Layout Selector"
                        bodyStyle={{ height: '554px', overflow: 'scroll' }}
                        extra={<LayoutSelector options={layouts} value={type} onChange={handleChange} />}
                    >
                        <Graphin data={layouteddata} layout={layout}></Graphin>
                    </Card>
        </div>
    );
}

export default LayoutedGraph;
