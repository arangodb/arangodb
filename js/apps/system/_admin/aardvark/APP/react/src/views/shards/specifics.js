import React, { Component } from 'react';
import styled from 'styled-components';
import {CheckCircle} from 'styled-icons/fa-solid';
import { Grid, Cell } from "styled-css-grid";
import { connect } from "react-redux";
import ShardInfoRow from "./shardInfoRow";
import {
  SHARDS_DETAILS_FETCH,
  SHARDS_DETAILS_UNSELECT,
} from "../../actions/actions";

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

const Header = styled(TableRow)`
border-bottom-color: rgba(140, 138, 137, 0.247059);
border-bottom-width: 1px;
border-bottom-style: solid;
font-weight: 600;
color: rgb(113, 125, 144);
`;
const Row = styled(TableRow)`
color: rgb(138, 150, 159);
font-weight: normal;
`;

const Name = styled(TableRow)`
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
const InSync = styled(CheckCircle)`
  color: #2ecc71;
  width: 12pt;
`;
const Percent = styled.div``;

const makeShards = (args) => {
  const { Plan, Current } = args;
  const shards = [];
  for (const [name, info] of Object.entries(Plan)) {
    if (!info.hasOwnProperty("progress")) {
      shards.push({name, sync: true, ...info });
    }
  }
  return shards;
};

const shardSorting = (a, b) => {
  const an = a.name;
  const bn = b.name;
  if (an < bn) { return -1; }
  if (an > bn) { return 1; }
  return 0;
}

const mapStateToProps = (state, own) => {
  const { shardDistribution } = state;
  const shards = makeShards(shardDistribution[own.name]);
  return { ...own, shards};
};

const mapDispatch = (dispatch, own) => {
  const { name } = own;
  return {
    select: () => { dispatch({payload: name, type: SHARDS_DETAILS_FETCH}); },
    deselect: () => { dispatch({payload: name, type: SHARDS_DETAILS_UNSELECT}); }
  };
};

class Specifics extends Component {

  render() {
    const { selected, name, shards } = this.props;
    if (!selected) {
      return (
        <List>
          <Name onClick={e => this.props.select()} columns={2}>
            <LeftAlign>{name}</LeftAlign>
            <RightAlign><InSync /></RightAlign>
          </Name>
        </List>
      );
    }
    return (
      <List>
        <Name onClick={e => this.props.deselect()} columns={2}>
          <LeftAlign>{name}</LeftAlign>
          <RightAlign><InSync /></RightAlign>
        </Name>
        <Header columns={4}>
          <LeftAlign>Shard</LeftAlign>
          <LeftAlign>Leader</LeftAlign>
          <LeftAlign>Followers</LeftAlign>
          <RightAlign>Sync</RightAlign>
        </Header>
        { shards.sort(shardSorting).map(s => <ShardInfoRow key={s.name} collection={name} {...s} />) }
      </List>
    )
  }
}

export default connect(mapStateToProps, mapDispatch)(Specifics);
