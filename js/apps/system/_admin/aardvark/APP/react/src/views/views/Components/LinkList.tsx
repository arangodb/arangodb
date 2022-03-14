import React, { Dispatch } from "react";
import { DispatchArgs } from "../../../utils/constants";
import { LinkProperties } from "../constants";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import { IconButton } from "../../../components/arango/buttons";
import Link from "./Link";
import { chain, isNull } from "lodash";

type CollProps = {
  links: {
    includeAllFields: boolean;
    analyzers: [];
    name: string;
    action: React.ReactElement;
  }[];
  addClick: React.MouseEventHandler<HTMLElement>;
  viewLink: (link: {} | []) => void;
  dispatch: Dispatch<DispatchArgs<LinkProperties>>;
  icon: string;
};

const LinkList: React.FC<CollProps> = ({
  links,
  addClick,
  icon,
  viewLink,
  dispatch
}) => {
  // const checkLinks = () => {
  //   if (links) {
  //     console.log(Object.values(links));
  //     Object.values(links).map((l, i) => {
  //       console.log(
  //         `Links: ${l.includeAllFields}, ${l.analyzers}, Index:  ${i}`
  //       );
  //     });
  //   }
  // };

  const removeLink = (collection: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: null
      }
    });
  };

  const getLinkRemover = (collection: string | number) => () => {
    removeLink(collection);
  };

  return (
    <div className="contentIn" id="indexHeaderContent">
      <ArangoTable className={"edit-index-table arango-table"}>
        <thead>
          <tr className="figuresHeader">
            <ArangoTH seq={0}>Collection Name</ArangoTH>
            <ArangoTH seq={1}>Properties</ArangoTH>
            <ArangoTH seq={1}>Root Analyzers</ArangoTH>
            <ArangoTH seq={3}>Action</ArangoTH>
          </tr>
        </thead>

        <tbody>
          {links &&
            Object.keys(links).length > 0 &&
            Object.values(links).map((c, key) => (
              <Link
                name={
                  chain(links)
                    .omitBy(isNull)
                    .keys()
                    .value()[0]
                }
                analyzers={c.analyzers}
                includeAllFields={c.includeAllFields}
                action={
                  <>
                    <IconButton
                      icon={"trash-o"}
                      type={"danger"}
                      onClick={getLinkRemover}
                    />
                    <IconButton
                      icon={"eye"}
                      type={"warning"}
                      onClick={() => viewLink(c)}
                    />
                  </>
                }
                key={key}
                linkKey={key}
              />
            ))}
        </tbody>
        <tfoot>
          <tr>
            <ArangoTD seq={0}> </ArangoTD>
            <ArangoTD seq={1}> </ArangoTD>
            <ArangoTD seq={2}> </ArangoTD>
            <ArangoTD seq={3}>
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

export default LinkList;
