import React, { Component, Dispatch } from 'react';
import { connect } from "react-redux";

import styled from 'styled-components';
import { CheckCircle, ArrowDown, ArrowRight } from 'styled-icons/fa-solid';
import { Grid, Cell } from "styled-css-grid";
import ShardInfoRow from "./shardInfoRow";

import { ClusterActions } from '../../store/cluster/types';
import { fetchShardDetails, unselectShardDetails } from '../../store/cluster/actions';
import { ApplicationState } from '../../store';

type ShardInfo = {
  name: string,
  leader: string,
  followers: string[],
  sync: boolean,
  CurrentFollowers: string[]
};

export interface IOwnProps {
  name: string,
  selected: boolean
};

interface IStateProps {
  shards: any,
};

interface IDispatchProps {
  select: () => void,
  deselect: () => void
}

type Props = IStateProps & IDispatchProps & IOwnProps;

interface IState {

}

const List = styled.div`
  font-family: sans-serif;
  font-size: 16px;
`;

const TableRow = styled(Grid)`
  line-height: 40px;
  margin-left: 20px;
  margin-right: 20px;
  padding-left: 20px;
  padding-right: 20px;
  margin-bottom: 1px;
`;

const Header = styled(TableRow as any)`
border-bottom-color: rgba(140, 138, 137, 0.247059);
border-bottom-width: 1px;
border-bottom-style: solid;
font-weight: 600;
color: rgb(113, 125, 144);
`;

const Name = styled(TableRow as any)`
  color: rgb(255, 255, 255);
  background-color: rgb(51, 51, 51);
  cursor: pointer;
`;

const LeftAlign = styled(Cell)`
text-align: left;
`;
const RightAlign = styled(Cell)`
text-align: right;
`;

const InSync = styled(CheckCircle as any)`
  color: #2ecc71;
  width: 12pt;
  padding: 0px 5px;
`;

const IsColapsed = styled(ArrowRight as any)`
  width: 12pt;
  padding: 0px 5px;
`;

const IsDetailed = styled(ArrowDown as any)`
  width: 12pt;
  padding: 0px 5px;
`;

const makeShards = (args : any) => {
  const { Plan } = args;
  const shards:Array<Object> = [];
  for (const [name, info] of Object.entries(Plan)) {
    if (!(info as object).hasOwnProperty("progress")) {
      shards.push({name, sync: true, ...(info as object) });
    }
  }
  return shards;
};

const shardSorting = (a: ShardInfo, b: ShardInfo) => {
  const an = a.name;
  const bn = b.name;
  if (an < bn) { return -1; }
  if (an > bn) { return 1; }
  return 0;
}

const mapStateToProps = (state : ApplicationState, own : IOwnProps) : IStateProps => {
  const { shardDistribution } = state.cluster;
  const shards = makeShards(shardDistribution[own.name]);
  return { ...own, shards};
};

const mapDispatch = (dispatch : Dispatch<ClusterActions>, own : IOwnProps) : IDispatchProps => {
  const { name } = own;
  return {
    select: () => { dispatch(fetchShardDetails(name)); },
    deselect: () => { dispatch(unselectShardDetails()); }
  };
};

class Specifics extends Component<Props, IState> {

  render() {
    const { selected, name, shards, select, deselect } = this.props;
    if (!selected) {
      return (
        <List>
          <Name onClick={(e : React.MouseEvent<HTMLDivElement>) => {e.preventDefault(); select()}} columns={2}>
            <LeftAlign>{name}</LeftAlign>
            <RightAlign><InSync /><IsColapsed /></RightAlign>
          </Name>
        </List>
      );
    }
    return (
      <List>
        <Name onClick={(e : React.MouseEvent<HTMLDivElement>)=> {e.preventDefault(); deselect()}} columns={2}>
          <LeftAlign>{name}</LeftAlign>
          <RightAlign><InSync /><IsDetailed /></RightAlign>
        </Name>
        <Header columns={4}>
          <LeftAlign>Shard</LeftAlign>
          <LeftAlign>Leader</LeftAlign>
          <LeftAlign>Followers</LeftAlign>
          <RightAlign>Sync</RightAlign>
        </Header>
        { shards.sort(shardSorting).map((s : ShardInfo) => <ShardInfoRow key={s.name} collection={name} {...s} />) }
      </List>
    )
  }
}

export default connect<IStateProps, IDispatchProps, IOwnProps, ApplicationState>(mapStateToProps, mapDispatch)(Specifics);
