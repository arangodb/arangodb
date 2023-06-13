import React from 'react';
import { getAdminRouteForCurrentDB } from '../../../utils/arangoClient';

async function rebalanceShards() {
  const route = getAdminRouteForCurrentDB();
  try {
    const res = await route.post("cluster/rebalanceShards");
    if (res.statusCode! >= 400 || !res.body?.result) {
      window.arangoHelper.arangoError('Could not start rebalance process.');
    } else if (res.body.result.operations === 0) {
      window.arangoHelper.arangoNotification('No move shards operations were scheduled.');
    } else {
      window.arangoHelper.arangoNotification('Started rebalance process. Scheduled ' + res.body.result.operations + ' shards move operation(s).');
    }
  } catch (e: any) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${e.message}`
    );
  }
}

export const RebalanceShards = ({ maxNumberOfMoveShards }: { maxNumberOfMoveShards: number }) => {
  return (
    <div className="arangoToolbar arangoToolbarTop" style={{ marginTop: "1rem" }} id="rebalanceShards">
      <div className="pull-right">
        <button id="rebalanceShardsBtn" style={{ marginLeft: "10px" }} className="button-warning" onClick={() => rebalanceShards()}>Rebalance Shards</button>
      </div>
      <div style={{ fontSize: "10pt", marginLeft: "10px", marginTop: "12px", fontWeight: 200 }}>
        <b>WARNING:</b> Clicking this button will start a
        shard rebalancing process and schedule up to {maxNumberOfMoveShards} shards move operations,
        which may cause background activity and increase the load in the cluster until finished.
      </div>
    </div>
  );
};
