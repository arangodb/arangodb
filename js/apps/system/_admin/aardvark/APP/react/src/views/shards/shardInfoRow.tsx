import React, { Component, Dispatch } from 'react';
import styled from 'styled-components';
import { CheckCircle } from 'styled-icons/fa-solid/CheckCircle';
import { Grid, Cell } from "styled-css-grid";
import { connect } from "react-redux";
import MoveShard from "./moveShard";
import { moveShard } from '../../store/cluster/actions';
import { ClusterActions } from '../../store/cluster/types';
import { ApplicationState } from '../../store';


const Leader = styled.span`
  color: #2ecc71;
`;


const Follower = styled.span<{inSync: boolean}>`
  color: ${props => props.inSync ? "#2ecc71" : "#FF0000" }
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
`;

/* const Percent = styled.div``;*/

const TableRow = styled(Grid)`
  line-height: 40px;
  margin-left: 20px;
  margin-right: 20px;
  padding-left: 20px;
  padding-right: 20px;
  margin-bottom: 1px;
`;

const Row = styled(TableRow as any)`
color: rgb(138, 150, 159);
font-weight: normal;
`;

interface IForwardedProps {
  name : string,
  collection : string,
  leader : string,
  followers : Array<string>,
  sync : boolean | number,
  CurrentFollowers : Array<string>
}

interface IOwnProps extends IForwardedProps {
};

interface IStateProps extends IForwardedProps {
  serverToShort : Map<string, string>,
  shortToServer : Map<string, string>
};

interface IDispatchProps {
  moveShard : (fromServer: string, toServer: string) => void
};

type Props = IStateProps & IDispatchProps;

interface IState {
  moving: string | null
}

const mapStateToProps = (state : ApplicationState , own : IOwnProps): IStateProps => {
  const { shardDistribution, serverToShort, shortToServer } = state.cluster;
  const { Current } = shardDistribution[own.collection];
  const CurrentFollowers = Current[own.name].followers;
  return { ...own, CurrentFollowers, serverToShort, shortToServer };
};

const mapDispatch = (dispatch : Dispatch<ClusterActions>, own : IOwnProps) => {
  const { name , collection } = own;
  return {
    moveShard: (fromServer : string, toServer : string) : void => {
      dispatch(moveShard({ collection, shard: name, fromServer, toServer }))
    }
  };
};

class ShardInfoRow extends Component<Props, IState> {
  state = {
    moving: ""
  }

  setMoving(source : string) {
    this.setState({moving: source});
  }

  cancelMove() {
    this.setState({moving: ""});
  }

  doMove(target : string) {
    const { moving } = this.state;
    this.props.moveShard(this.shortNameToServer(moving), this.shortNameToServer(target));
    this.setState({moving: ""});
  }

  serverToShortName = (server : string) : string => {
    const { serverToShort } = this.props;
    const name = serverToShort.get(server);
    if (name) {
      return name;
    }
    return server;
  };

  shortNameToServer = (shortName : string) : string => {
    const { shortToServer } = this.props;
    const name = shortToServer.get(shortName);
    if (name) {
      return name;
    }
    return shortName;
  }

  render()  {
    const { name, leader, followers, sync, CurrentFollowers } = this.props;
    const { moving } = this.state;
    if (moving) {
      return (
        <MoveShard name={name} leader={leader} followers={followers} source={moving} doMove={this.doMove.bind(this)} doCancel={this.cancelMove.bind(this)} />
      );
    }
    return (
      <Row key={name} columns={4}>
        <LeftAlign>{name}</LeftAlign>
        <LeftAlign><Leader onClick={e => {e.preventDefault(); this.setMoving(leader)}} key={leader}>{leader}</Leader></LeftAlign>
        <LeftAlign>
          {followers.map((f,i) => (
            <Follower key={f} inSync={CurrentFollowers.indexOf(f) > -1}>{i > 0 ? ", " : ""}{f}</Follower>
            ))}
        </LeftAlign>
        <RightAlign>{sync === true ? (<InSync />) : <InSync />}</RightAlign>
      </Row>
    );
  }
}
/*<Percent> {sync.toFixed(2)}% </Percent> */
  /* <RightAlign>{sync === true ? (<InSync />) : <Percent> {sync.toFixed(2)}% </Percent>}</RightAlign>*/
export default connect<IStateProps, IDispatchProps, IOwnProps, ApplicationState>(mapStateToProps, mapDispatch)(ShardInfoRow);
