import { Switch, Input, Space, Tag, Card, Select, PageHeader, Tabs, Button, Statistic, Descriptions, Tooltip } from 'antd';
import GraphDataInfo from './GraphDataInfo';
import { InfoCircleOutlined, SaveOutlined, NodeIndexOutlined, NodeExpandOutlined, DownloadOutlined, FullscreenOutlined, ShareAltOutlined, CameraOutlined } from '@ant-design/icons';

export const Headerinfo = ({ graphName }) => {
  
  const { Option } = Select;
  const { TabPane } = Tabs;

  function handleChange(value) {
    console.log(`selected ${value}`);
  }

  function onSwitchChange(checked) {
    console.log(`switch to ${checked}`);
  }

  const renderContent = (column = 2) => (
    <>
      <Descriptions size="small" column={column}>
        <Descriptions.Item label="Response time">[12]ms
        </Descriptions.Item>
      </Descriptions>
      <Tag color="cyan">[4] nodes</Tag>
      <Tag color="magenta">[12] nodes</Tag>
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
      <Button type="primary" icon={<SaveOutlined />}>
        Save
      </Button>
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
            <Button key="4"><DownloadOutlined /></Button>
          </Tooltip>
          <Tooltip placement="bottom" title={"Download visible graph as screenshot"}>
            <Button key="3"><CameraOutlined /></Button>
          </Tooltip>
          <Tooltip placement="bottom" title={"Switch to fullscreen mode"}>
            <Button key="2"><FullscreenOutlined /></Button>
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
            <Input
              addonBefore="Startnode"
              placeholder="Startnode"
              suffix={
                <Tooltip title="A valid node id or space seperated list of id's. If empty a random node will be chosen.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
            <br />
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
            <Input
              addonBefore="Search depth"
              placeholder="Search depth"
              suffix={
                <Tooltip title="Search depth, starting from your startnode">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px' }}
            />
            <br />
            <Input
              addonBefore="Limit"
              placeholder="Limit"
              suffix={
                <Tooltip title="Limit nodes count. If empty or zero, no limit is set.">
                  <InfoCircleOutlined style={{ color: 'rgba(0,0,0,.45)' }} />
                </Tooltip>
              }
              style={{ width: 240, marginTop: '24px', marginBottom: '24px' }}
            />
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
              suffix={
                <Tooltip title="A valid node id or space seperated list of id's. If empty a random node will be chosen.">
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
            <br />
            <Switch
              checkedChildren="Color by collections"
              unCheckedChildren="Don't color by collections"
              onChange={onSwitchChange}
              style={{ marginTop: '24px' }}
            />
            <br />
            <Input
              addonBefore="Color"
              placeholder="Color"
              suffix={
                <Tooltip title="A valid node id or space seperated list of id's. If empty a random node will be chosen.">
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
                <Tooltip title="Search depth, starting from your startnode">
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
            <br />
            <Input
              addonBefore="Sizing attribute"
              placeholder="Attribute"
              suffix={
                <Tooltip title="Search depth, starting from your startnode">
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
                <Tooltip title="A valid node id or space seperated list of id's. If empty a random node will be chosen.">
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
            <br />
            <Switch
              checkedChildren="Color by collections"
              unCheckedChildren="Don't color by collections"
              onChange={onSwitchChange}
              style={{ marginTop: '24px' }}
            />
            <br />
            <Input
              addonBefore="Color"
              placeholder="Color"
              suffix={
                <Tooltip title="A valid node id or space seperated list of id's. If empty a random node will be chosen.">
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
                <Tooltip title="Search depth, starting from your startnode">
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
          </TabPane>
        </Tabs>
      }
    >
      <Content>{renderContent()}</Content>
    </PageHeader>
  );
}
