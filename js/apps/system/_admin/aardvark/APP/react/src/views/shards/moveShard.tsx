import React, { Component } from 'react';
import styled from 'styled-components';
import { Grid, Cell } from "styled-css-grid";
import { connect } from "react-redux";
import { ApplicationState } from '../../store';

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

const LeftAlign = styled(Cell)`
text-align: left;
`;
const RightAlign = styled(Cell)`
text-align: right;
`;

const Button = styled.button``;

interface IForwardedProps {
  name: string,
  source: string,
  doMove: (name: string) => void,
  doCancel: () => void
}

interface IOwnProps extends IForwardedProps {
  leader: string,
  followers: string[]
};

interface IStateProps extends IForwardedProps {
  candidates: string[],
};

interface IDispatchProps { };

type Props = IStateProps & IDispatchProps;

interface IState {

}

const mapStateToProps = (state : ApplicationState, own : IOwnProps) : IStateProps => {
  const { shortToServer } = state.cluster;
  const { leader, followers, name, source, doMove, doCancel } = own;
  const candidates = Array.from(shortToServer.keys())
    .filter((n : string ) : boolean => n.startsWith('DB'))
    .filter((n : string ) : boolean => n !== leader)
    .filter((n : string) : boolean => -1 === followers.indexOf(n))
    .sort();
  return { candidates, name, source ,doMove, doCancel};
};

class MoveShard extends Component<Props, IState> {
  state = {
    selected: ""
  }

  selectTarget(evt : React.ChangeEvent<HTMLSelectElement>) {
    evt.preventDefault();
    this.setState({selected: evt.target.value});
  }

  componentDidMount() {
    const { candidates } = this.props;
    if (!this.state.selected) {
      this.setState({selected: candidates[0]});
    }
  }
  
  render() {
    const { candidates, name, source, doMove, doCancel } = this.props;
    if (candidates.length === 0) {
      return (
        <Row key={name} columns={2}>
          <LeftAlign>Shard cannot be moved, it is on all servers</LeftAlign>
          <RightAlign>
            <Button onClick={e => {e.preventDefault(); doCancel()}}>Cancel</Button>
          </RightAlign>
        </Row>
      )
    }
    return (
      <Row key={name} columns={4}>
        <LeftAlign>Move Shard: {name}</LeftAlign>
        <LeftAlign>From: {source}</LeftAlign>
        <LeftAlign>To:
          <select onChange={e => this.selectTarget(e)}>
            { candidates.map(c => (<option key={c} value={c}>{c}</option>)) }
          </select>
        </LeftAlign>
        <RightAlign>
          <Button onClick={e => {e.preventDefault(); doCancel()}}>Cancel</Button>
          <Button onClick={e => {e.preventDefault(); doMove(this.state.selected)}}>Save</Button>
        </RightAlign>
      </Row>
    );
  }
}

export default connect<IStateProps, IDispatchProps, IOwnProps, ApplicationState>(mapStateToProps)(MoveShard);
