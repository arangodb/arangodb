import React, { Component } from 'react';
import styled from 'styled-components';
import {CheckCircle} from 'styled-icons/fa-solid';
import { Grid, Cell } from "styled-css-grid";
import { connect } from "react-redux";

const Leader = styled.span`
  color: #2ecc71;
`;

const Follower = styled.span`
  color: #2ecc71;
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

const TableRow = styled(Grid)`
  line-height: 40px;
  margin-left: 20px;
  margin-right: 20px;
  padding-left: 20px;
  padding-right: 20px;
  margin-bottom: 1px;
`;

const Row = styled(TableRow)`
color: rgb(138, 150, 159);
font-weight: normal;
`;

const mapStateToProps = (state, own) => {
  const { shardDistribution } = state;
  const { Current } = shardDistribution[own.collection];
  const CurrentFollowers = Current[own.name].followers;
  return { ...own, CurrentFollowers};
};

class ShardInfoRow extends Component {

  render()  {
    const {name, leader, followers, sync, CurrentFollowers} = this.props;
    return (
      <Row key={name} columns={4}>
        <LeftAlign>{name}</LeftAlign>
        <LeftAlign><Leader key={leader}>{leader}</Leader></LeftAlign>
        <LeftAlign>{followers.map((f,i) => (<Follower key={f} inSync={CurrentFollowers.indexOf(f) > -1}>{i > 0 ? ", " : ""}{f}</Follower>))}</LeftAlign>
        <RightAlign>{sync === true ? <InSync /> : <Percent> {sync.toFixed(2)}% </Percent>}</RightAlign>
      </Row>
    );
  }
}

export default connect(mapStateToProps)(ShardInfoRow);
