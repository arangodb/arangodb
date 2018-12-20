import React, { Component } from 'react';
import { connect } from 'react-redux';
import Specifics from './specifics';

import { div } from 'styled-components';

const mapStateToProps = state => {
  const { shardDistribution, selected } = state;
  return { shardDistribution, selected };
};


class Overview extends Component {
  render() {
    const { shardDistribution, selected } = this.props;
    return (
      <div>
      { Object.keys(shardDistribution)
        .filter(name => name.charAt(0) !== '_')
        .sort()
        .map( name => <Specifics key={name} name={name} selected={selected === name} />) }
      </div>
    );
  }
}
export default connect(mapStateToProps)(Overview);
