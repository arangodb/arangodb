/* global $, arangoHelper */
import React from 'react';

function rebalanceShards() {
  $.ajax({
    type: 'POST',
    cache: false,
    url: arangoHelper.databaseUrl('/_admin/cluster/rebalanceShards'),
    contentType: 'application/json',
    processData: false,
    data: JSON.stringify({}),
    async: true,
    success: function (data) {
      if (data.result.operations === 0) {
        arangoHelper.arangoNotification('No move shards operations were scheduled.');
      } else {
        arangoHelper.arangoNotification('Started rebalance process. Scheduled ' + data.result.operations + ' shards move operation(s).');
      }
    },
    error: function () {
      arangoHelper.arangoError('Could not start rebalance process.');
    }
  });
  window.modalView.hide();
}

export const RebalanceShards = ({ maxNumberOfMoveShards }) => {
  return (
    <div className="arangoToolbar arangoToolbarTop" style={{ marginTop: "1rem" }} id="rebalanceShards">
      <div className="pull-right">
        <button id="rebalanceShardsBtn" style={{ marginLeft: "10px" }} className="button-warning" onClick={() => rebalanceShards()}>Rebalance Shards</button>
      </div>
      <div style={{ fontSize: "10pt", marginLeft: "10px", marginTop: "12px", fontWeight: "200" }}>
        <b>WARNING:</b> Clicking this button will start a
        shard rebalancing process and schedule up to {maxNumberOfMoveShards} shards move operations,
        which may cause background activity and increase the load in the cluster until finished.
      </div>
    </div>
  );
};
