import React, { useCallback, useEffect, useState } from 'react';
import { Switch, Input, Space, Tag, Card, Select, PageHeader, Tabs, Button, Statistic, Descriptions, Tooltip } from 'antd';
import GraphDataInfo from './GraphDataInfo';
import { InfoCircleOutlined, SaveOutlined, NodeIndexOutlined, NodeExpandOutlined, DownloadOutlined, FullscreenOutlined, ShareAltOutlined, CameraOutlined, SearchOutlined } from '@ant-design/icons';
import LayoutSelector from './LayoutSelector.js';
import { ResponseInfo } from './ResponseInfo';
import { data2 } from './data2';
import { NodeList } from './components/node-list/node-list.component';
import { EdgeList } from './components/edge-list/edge-list.component';
import ParameterNodeStart from "./ParameterNodeStart";
import ParameterDepth from "./ParameterDepth";
import ParameterLimit from "./ParameterLimit";
import ButtonSave from "./ButtonSave";

export const Headerinfo = ({ graphName, graphData, responseDuration, onDownloadScreenshot, onChangeLayout, onChangeGraphData, onLoadFullGraph, onDocumentSelect, onGraphDataLoaded }) => {
  
  const [layout, setLayout] = useState('gForce');
  const { Option } = Select;
  const { TabPane } = Tabs;

  function handleChange(value) {
    console.log(`selected ${value}`);
  }

  function onSwitchChange(checked) {
    console.log(`switch to ${checked}`);
  }

  const changeLayout = ({ newLayout }) => {
    /*
    this.graph.updateLayout({
      type: value,
    });
    */
   console.log("New graph newLayout: ", newLayout);
  }

  const renderContent = (column = 2) => (
    <>
      <Descriptions size="small" column={column}>
        <Descriptions.Item label="Response time">
          <ResponseInfo
            duration={responseDuration}
          />ms
        </Descriptions.Item>
      </Descriptions>
      <Tag color="cyan">{graphData.nodes.length} nodes</Tag>
      <Tag color="magenta">{graphData.edges.length} edges</Tag>
    </>
  );

  const Content = ({ children, extra }) => (
    <div className="content">
      <div className="main">{children}</div>
      <div className="extra">{extra}</div>
    </div>
  );

  const menuActionButtons = <>
    <Space>
      <ButtonSave onGraphDataLoaded={(newGraphData) => onGraphDataLoaded(newGraphData)}/>
      <Button>
        Restore default values
      </Button>
    </Space>
  </>;

  return (
    <PageHeader
      className="site-page-header-responsive"
      onBack={() => window.history.back()}
      title={graphName}
      subTitle=""
      extra={[
        <>
          <Tooltip placement="bottom" title={"Fetch full graph - use with caution"}>
            <Button key="4" onClick={onLoadFullGraph()}><DownloadOutlined /></Button>
          </Tooltip>
          <Tooltip placement="bottom" title={"Download visible graph as screenshot"}>
            <Button key="3" onClick={() => {
              console.log("Screenshot button clicked");
              onDownloadScreenshot();
            }}><CameraOutlined /></Button>
          </Tooltip>
          <Tooltip placement="bottom" title={"Switch to fullscreen mode"}>
            <Button key="2"
              onClick={() => {
                const elem = document.getElementById("graph-card");
                if (elem.requestFullscreen) {
                  elem.requestFullscreen();
                }}}><FullscreenOutlined />
            </Button>
          </Tooltip>
          <Button key="1">
            Primary
          </Button>
        </>
      ]}
      ghost={false}
      footer={
        <Tabs defaultActiveKey="1" type="card" size="small" tabBarExtraContent={menuActionButtons}>
          <TabPane
            tab={
              <span>
                <ShareAltOutlined />
                Graph
              </span>
            }
            key="1"
          >
            <ParameterNodeStart />
            <br />
            <LayoutSelector
              value={layout}
              onChange={(value) => changeLayout}
              onLayoutChange={(layoutvalue) => {
                console.log("New layout value in Headerinfo: ", layoutvalue);
                onChangeLayout(layoutvalue);
              }} />
            <Select
              defaultValue="lucy"
              style={{ width: 240, marginTop: '24px' }}
              onChange={handleChange}
              suffix={
                <Tooltip title="Different graph algorithms. No overlap is very fast (more than 5000 nodes), force is slower (less than 5000 nodes) and fruchtermann is the slowest (less than 500 nodes).">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }>
              <Option value="jack">Jack</Option>
              <Option value="lucy">Lucy</Option>
              <Option value="disabled" disabled>
                Disabled
              </Option>
              <Option value="Yiminghe">yiminghe</Option>
            </Select>
            <br />
            <ParameterDepth />
            <br />
            <ParameterLimit />
            <br />
          </TabPane>
          <TabPane
            tab={
              <span>
                <NodeExpandOutlined />
                Nodes
              </span>
            }
            key="2"
          >
            <Input
              addonBefore="Label"
              placeholder="Attribute"
              suffix = {
                <Tooltip title="Node label. Please choose a valid and available node attribute.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
            <br />
            <Switch
              checkedChildren="Show collection name"
              unCheckedChildren="Hide collection name"
              onChange={onSwitchChange}
              style={{ marginTop: '24px' }}
            />
            <Tooltip title="Append collection name to the label?">
              <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
            </Tooltip>
            <br />
            <Switch
              checkedChildren="Color by collections"
              unCheckedChildren="Don't color by collections"
              onChange={onSwitchChange}
              style={{ marginTop: '24px' }}
            />
            <Tooltip title="Should nodes be colorized by their collection? If enabled, node color and node color attribute will be ignored.">
              <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
            </Tooltip>
            <br />
            <Input
              addonBefore="Color"
              placeholder="Color"
              suffix = {
                <Tooltip title="Default node color. RGB or HEX value.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
            <br />
            <Input
              addonBefore="Color attribute"
              placeholder="Attribute"
              suffix={
                <Tooltip title="If an attribute is given, nodes will then be colorized by the attribute. This setting ignores default node color if set.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
            <br />
            <Switch
              checkedChildren="Size by connections"
              unCheckedChildren="Default size"
              onChange={onSwitchChange}
              style={{ marginTop: '24px' }}
            />
            <Tooltip title="Should nodes be sized by their edges count? If enabled, node sizing attribute will be ignored.">
              <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
            </Tooltip>
            <br />
            <Input
              addonBefore="Sizing attribute"
              placeholder="Attribute"
              suffix={
                <Tooltip title="Default node size. Numeric value > 0.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px', marginBottom: '24px' }}
            />
          </TabPane>
          <TabPane
            tab={
              <span>
                <NodeIndexOutlined />
                Edges
              </span>
            }
            key="3"
          >
            <Input
              addonBefore="Label"
              placeholder="Attribute"
              suffix={
                <Tooltip title="Default edge label">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
            <br />
            <Switch
              checkedChildren="Show collection name"
              unCheckedChildren="Hide collection name"
              onChange={onSwitchChange}
              style={{ marginTop: '24px' }}
            />
            <Tooltip title="Set label text by collection. If activated edge label attribute will be ignored.">
              <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
            </Tooltip>
            <br />
            <Switch
              checkedChildren="Color by collections"
              unCheckedChildren="Don't color by collections"
              onChange={onSwitchChange}
              style={{ marginTop: '24px' }}
            />
            <Tooltip title="Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored.">
              <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)', marginTop: '24px' }} />
            </Tooltip>
            <br />
            <Input
              addonBefore="Color"
              placeholder="Color"
              suffix={
                <Tooltip title="Default node color. RGB or HEX value.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
            <br />
            <Input
              addonBefore="Color attribute"
              placeholder="Attribute"
              suffix={
                <Tooltip title="If an attribute is given, edges will then be colorized by the attribute. This setting ignores default edge color if set.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
            <br />
            <Select
              defaultValue="lucy"
              style={{ width: 240, marginBottom: '24px' , marginTop: '24px' }}
              onChange={handleChange}
              suffix={
                <Tooltip title="The type of the edge">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }>
              <Option value="jack">Jack</Option>
              <Option value="lucy">Lucy</Option>
              <Option value="disabled" disabled>
                Disabled
              </Option>
              <Option value="Yiminghe">yiminghe</Option>
            </Select>
          </TabPane>
          <TabPane
            tab={
              <span>
                <SearchOutlined />
                Search
              </span>
            }
            key="4"
          >
            <NodeList
              nodes={graphData.nodes}
              graphData={graphData}
              onNodeInfo={() => console.log('onNodeInfo() in MenuGraph')}
              onNodeSelect={(node) => onDocumentSelect(node)}
            />
            <EdgeList
              edges={graphData.edges}
              graphData={graphData}
              onEdgeSelect={(edge) => onDocumentSelect(edge)}
              />
          </TabPane>
        </Tabs>
      }
    >
      <Content>{renderContent()}</Content>
    </PageHeader>
  );
}
