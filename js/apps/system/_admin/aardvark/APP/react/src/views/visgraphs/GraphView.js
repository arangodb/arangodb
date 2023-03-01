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
        nodesColorAttributes={this.props.nodesColorAttributes}
        onChangeGraphData={(newGraphData) => {
          this.props.onChangeGraphData(newGraphData);
        }}
        onLoadFullGraph={() => this.props.onLoadFullGraph}
        onGraphDataLoaded={(newGraphData, responseTimesObject) => {
          this.props.onGraphDataLoaded({newGraphData, responseTimesObject})}}
      />

      <VisNetwork
        graphData={this.props.visGraphData}
        graphName={this.props.graphName}
        options={{}}
      />

      <GraphInfo>
        <Tag label={`${this.props.visGraphData.nodes.length} nodes`}/><Tag label={`${this.props.visGraphData.edges.length} edges`}/><Tag template='transparent' label={`Response time: ${this.props.responseDuration}ms`}/>
      </GraphInfo>
    </>;
  }
}
