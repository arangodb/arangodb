import React from "react";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import Link from "./Link";

interface CollProps {
  links: {
    attr: string;
    name: string;
    desc: string;
    action: React.ReactElement;
  }[];
}

const CollectionList = (links: CollProps["links"]) => {
  return (
    <div className="contentIn" id="indexHeaderContent">
      <ArangoTable className={"edit-index-table arango-table"}>
        <thead>
          <tr className="figuresHeader">
            <ArangoTH seq={0}>ID</ArangoTH>
            <ArangoTH seq={1}>Attributes</ArangoTH>
            <ArangoTH seq={2}>Name</ArangoTH>
            <ArangoTH seq={3}>Options</ArangoTH>
            <ArangoTH seq={4}>Action</ArangoTH>
          </tr>
        </thead>

        <tbody>
          {links.map((c, key) => (
            <Link
              name={c.name}
              attr={c.attr}
              desc={c.attr}
              action={<i className="fa fa-plus-circle"></i>}
              key={key}
            />
          ))}
        </tbody>
        <tfoot>
          <tr>
            <ArangoTD seq={0}> </ArangoTD>
            <ArangoTD seq={1}> </ArangoTD>
            <ArangoTD seq={2}> </ArangoTD>
            <ArangoTD seq={3}> </ArangoTD>
            <ArangoTD seq={4}>
              <i className="fa fa-plus-circle"></i>
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
