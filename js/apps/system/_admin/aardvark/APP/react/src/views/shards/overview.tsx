import React, { Component, Dispatch } from 'react';
import { connect } from 'react-redux';
import Specifics from './specifics';
import styled from 'styled-components';
import { ApplicationState } from '../../store';
import { ClusterActions } from '../../store/cluster/types';
import { rebalanceShards, fetchShardOverview } from '../../store/cluster/actions';
import { taskRepeater, Task } from '../../actions/taskRepeater';

type OverviewProps = {
  shards: Array<string>,
  selected : string
};

type OverviewActions = {
  rebalanceShards: () => void,
  fetchShardOverview: () => void
};

type OverviewPropsAndActions = OverviewProps & OverviewActions;

const SuccessButton = styled.button`
  float: right;
  background-color: green;
  color: white;
`;

const mapStateToProps = (state : ApplicationState) : OverviewProps => {
  const { shardDistribution, selected } = state.cluster;
  const shards = Object.keys(shardDistribution)
    /* .filter(name => name.charAt(0) !== '_') */
    .sort();
  return { shards, selected };
};

const mapDispatch = (dispatch : Dispatch<ClusterActions>) : OverviewActions => {
  return {
    rebalanceShards: () : void => { dispatch(rebalanceShards()); },
    fetchShardOverview: () : void => { dispatch(fetchShardOverview()); }
  };
};

class ShardUpdateTask implements Task {
  private call : () => void;

  constructor(call : () => void) {
   this.call = call;
  }

  execute() {
    return this.call();
  }
}

class Overview extends Component<OverviewPropsAndActions> {

  componentWillMount() {
    const { fetchShardOverview } = this.props;
    taskRepeater.registerTask("updateShardList", new ShardUpdateTask(fetchShardOverview));
  }

  componentWillUnmount() {
    taskRepeater.stopTask("updateShardList");
  }

  render() {
    const { shards, selected } = this.props;
    return (
      <div>
        { shards.map( name => <Specifics key={name} name={name} selected={selected === name} />) }
        <SuccessButton onClick={ e => {e.preventDefault(); this.props.rebalanceShards()}} >
          RebalanceShards
        </SuccessButton>
      </div>
    );
  }
}
export default connect(mapStateToProps, mapDispatch)(Overview);
