import React from 'react';
import { Headerinfo } from './Headerinfo';
import './graphview.menu.css';
import './visgraphstyles.css'
import VisNetwork from './VisNetwork';
import Tag from "../../components/pure-css/form/Tag";
import GraphInfo from './components/pure-css/form/GraphInfo';

export class GraphView extends React.Component {

  shouldComponentUpdate(nextProps, nextState) {
    if (this.props.visGraphData === nextProps.visGraphData) {
      return false;
    } else {
      return true;
    }
  }

  render() {
    return <>
      <Headerinfo
        graphName={this.props.graphName}
        graphData={this.props.data}
        responseDuration={this.props.responseDuration}
        onChangeGraphData={(newGraphData) => {
          this.props.onChangeGraphData(newGraphData);
        }}
        onGraphDataLoaded={(newGraphData, responseTimesObject) => {
          this.props.onGraphDataLoaded({newGraphData, responseTimesObject})}}
      />

      <VisNetwork
        graphData={this.props.visGraphData}
        graphName={this.props.graphName}
        options={{}}
        onSelectNode={(nodeId) => {
          this.props.onClickNode(nodeId);
        }}
        onSelectEdge={(edgeId) => {
          this.props.onClickEdge(edgeId);
        }}
        onDeleteNode={(nodeId) => {
          console.log("nodeId: ", nodeId);
          this.props.onDeleteNode(nodeId);
        }}
        onDeleteEdge={(edgeId) => {
          this.props.onDeleteEdge(edgeId);
        }}
        onEditNode={(nodeId) => {
          this.props.onEditNode(nodeId);
        }}
        onEditEdge={(edgeId) => {
          this.props.onEditEdge(edgeId);
        }}
        onSetStartnode={(nodeId) => {
          this.props.onSetStartnode(nodeId);
        }}
        onExpandNode={(nodeId) => {
          this.props.onExpandNode(nodeId);
        }}
        onAddNodeToDb={() => {
          this.props.onAddNodeToDb();
        }}
        onAddEdge={(edgeData) => {
          const id = (Math.random() + 1).toString(36).substring(7);
          edgeData.id = id;
          this.props.onAddEdgeToDb(edgeData);
        }}
      />

      <GraphInfo>
        <Tag label={`${this.props.visGraphData.nodes.length} nodes`}/><Tag label={`${this.props.visGraphData.edges.length} edges`}/><Tag template='transparent' label={`Response time: ${this.props.responseDuration}ms`}/>
      </GraphInfo>
    </>;
  }
}
