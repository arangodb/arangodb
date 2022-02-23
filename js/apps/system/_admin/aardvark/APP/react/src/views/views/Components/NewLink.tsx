import React from "react";
import { ArangoTable, ArangoTH } from "../../../components/arango/table";
import ToolTip from "../../../components/arango/tooltip";

const NewList: React.FC = () => {
  return (
    <div className="contentIn" id="indexHeaderContent">
      <div id="newIndexView" className="new-index-view">
        <ArangoTable>
          <tbody>
            <tr>
              <ArangoTH seq={0} className="collectionTh">
                Type:
              </ArangoTH>
              <ArangoTH seq={1} className="">
                <select id="newIndexType">
                  <option value="unknown"> -- select a collection --</option>
                </select>
              </ArangoTH>
              <ArangoTH seq={2} className="tooltipInfoTh">
                <ToolTip
                  msg="Type of index to create. Please note that for the RocksDB
              engine the index types hash, skiplist and persistent are identical,
              so that they are not offered seperately here."
                  icon="icon_arangodb_info"
                  placement="left"
                />
              </ArangoTH>
            </tr>
          </tbody>
        </ArangoTable>
      </div>
    </div>
  );
};

export default NewList;
