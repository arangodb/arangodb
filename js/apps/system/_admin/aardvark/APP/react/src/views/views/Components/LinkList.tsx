import React from "react";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import Link from "./Link";

type CollProps = {
  links: {
    properties: boolean[];
    name: string;
    action: React.ReactElement;
  }[];
  addClick: React.MouseEventHandler<HTMLElement>;
  icon: string;
};

const CollectionList: React.FC<CollProps> = ({ links, addClick, icon }) => {
  return (
    <div className="contentIn" id="indexHeaderContent">
      <ArangoTable className={"edit-index-table arango-table"}>
        <thead>
          <tr className="figuresHeader">
            <ArangoTH seq={0}>Collection Name</ArangoTH>
            <ArangoTH seq={1}>Properties</ArangoTH>
            <ArangoTH seq={3}>Action</ArangoTH>
          </tr>
        </thead>

        <tbody>
          {links &&
            links.map((c, key) => (
              <Link
                name={c.name}
                properties={c.properties}
                action={<i className="fa fa-trash-circle"></i>}
                key={key}
              />
            ))}
        </tbody>
        <tfoot>
          <tr>
            <ArangoTD seq={0}> </ArangoTD>
            <ArangoTD seq={1}> </ArangoTD>
            <ArangoTD seq={2}>
              <i className={`fa ${icon}`} onClick={addClick}></i>
            </ArangoTD>
          </tr>
        </tfoot>
      </ArangoTable>

      <div id="modal-dialog">
        <div className="modal-footer" style={{ border: "none" }} />
      </div>
    </div>
  );
};

export default CollectionList;
