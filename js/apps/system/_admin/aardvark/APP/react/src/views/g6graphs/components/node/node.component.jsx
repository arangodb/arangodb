import React from 'react';
import {
  TagFilled,
  DeleteFilled,
  ExpandAltOutlined,
  TrademarkCircleFilled,
  ChromeFilled,
  BranchesOutlined,
  ApartmentOutlined,
  AppstoreFilled,
  CopyrightCircleFilled,
  CustomerServiceFilled,
  ShareAltOutlined,
  EditOutlined,
  FlagOutlined,
  InfoCircleOutlined
} from '@ant-design/icons';

export const Node = ({ node, onNodeInfo, onNodeEdit, onDeleteNode, onSetStartNode, onExpandNode }) => {

  const showNodeInfo = () => {
    console.log("SHOW NODE INFO IN MODAL: " , node);
  }

  /*
  <button
    onClick={() => showNodeInfo}>
    <div><InfoCircleOutlined className="inline-block align-text-top" /></div>
  </button>
  <button
    onClick={() => onNodeEdit(node)}>
    <div><EditOutlined className="inline-block align-text-top" /></div>
  </button>
  <button
    onClick={() => onDeleteNode}>
    <div><DeleteFilled className="inline-block align-text-top" /></div>
  </button>
  <button
    onClick={() => onSetStartNode}>
    <div><FlagOutlined className="inline-block align-text-top" /></div>
  </button>
  <button
    onClick={() => onExpandNode}>
    <div><ExpandAltOutlined className="inline-block align-text-top" /></div>
  </button>
  */
  return (
    <option value={node.id} />
  );
}
