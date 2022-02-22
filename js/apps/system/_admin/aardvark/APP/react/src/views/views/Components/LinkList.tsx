import React from "react";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";

interface CollProps {
  links: {
    attr: string;
    name: string;
    desc: string;
    action: React.ReactElement;
  }[];
}

const CollectionList: React.FC<CollProps["links"]> = links => {
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
            <tr>
              <ArangoTD seq={key} key={key}>
                {key}
              </ArangoTD>
              <ArangoTD seq={key} key={key}>
                {c.attr}
              </ArangoTD>
              <ArangoTD seq={key} key={key}>
                {c.name}
              </ArangoTD>
              <ArangoTD seq={key} key={key}>
                {c.desc}
              </ArangoTD>
              <ArangoTD seq={key} key={key}>
                {c.action}
              </ArangoTD>
            </tr>
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
