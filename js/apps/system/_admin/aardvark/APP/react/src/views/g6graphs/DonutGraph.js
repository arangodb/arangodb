import React from 'react';
import Graphin, { Utils, GraphinContext } from '@antv/graphin';
import { message, Card } from 'antd';
import { ContextMenu, FishEye } from '@antv/graphin-components';
import {
  TagFilled,
  DeleteFilled,
  ExpandAltOutlined
} from '@ant-design/icons';

const DonutGraph = () => {
  
  const { Menu, Donut } = ContextMenu;
    const options = [
        {
            key: 'tag',
            icon: <TagFilled />,
            name: 'Tag',
        },
        {
            key: 'delete',
            icon: <DeleteFilled />,
            name: 'Delete',
        },
        {
            key: 'expand',
            icon: <ExpandAltOutlined />,
            name: 'Expand',
        },
    ];
    const CanvasMenu = props => {
        const { graph, contextmenu } = React.useContext(GraphinContext);
        const context = contextmenu.canvas;
        const handleDownload = () => {
            graph.downloadFullImage('canvas-contextmenu');
            context.handleClose();
        };
        const handleClear = () => {
            message.info(`Canvas successfully deleted`);
            context.handleClose();
        };
        const handleStopLayout = () => {
            message.info(`Layout successfully stopped`);
            context.handleClose();
        };
        const handleOpenFishEye = () => {
            props.handleOpenFishEye();
        };
        return (
            <Menu bindType="canvas">
                <Donut.Item onClick={handleOpenFishEye}>Open Fisheye</Donut.Item>
                <Donut.Item onClick={handleClear}>Clear Canvas</Donut.Item>
                <Donut.Item onClick={handleStopLayout}>Stop layout</Donut.Item>
                <Donut.Item onClick={handleDownload}>Download Screenshot</Donut.Item>
            </Menu>
        );
    };

    const DonutGraphContainer = () => {
        const [visible, setVisible] = React.useState(false);
        const handleOpenFishEye = () => {
            setVisible(true);
        };
        const handleClose = () => {
            setVisible(false);
        };

        const data = Utils.mock(5)
            .circle()
            .graphin();
        const handleChange = (menuItem, menuData) => {
            console.log(menuItem, menuData);
            message.info(`Element：${menuData.id}，Action：${menuItem.name}`);
        };

        return (
            <div>
                        <Card title="Rightclick-menu + Fisheye">
                            <Graphin data={data}>
                            </Graphin>
                        </Card>
            </div>
        );
    }

    return <div><DonutGraphContainer /></div>;
}

export default DonutGraph;
