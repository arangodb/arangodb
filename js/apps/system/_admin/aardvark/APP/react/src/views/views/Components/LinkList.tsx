import React, { MouseEventHandler, useContext } from "react";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import { IconButton } from "../../../components/arango/buttons";
import Link from "./Link";
import { map } from "lodash";
import { ViewContext } from "../ViewLinksReactView";

type CollProps = {
  links: {};
  addClick: MouseEventHandler<HTMLElement>;
  viewLink: (link: {} | []) => void;
  icon: string;
};

const LinkList = ({ links, addClick, icon, viewLink }: CollProps) => {
  const checkLinks = (links: any) => {
    let linksArr = [];
    if (links) {
      for (const l in links) {
        linksArr.push({ name: l, link: links[l] });
      }
    }
    return linksArr;
  };

  const linksArr = checkLinks(links);

  const { dispatch } = useContext(ViewContext);

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
            <ArangoTH seq={0}>Link Name</ArangoTH>
            <ArangoTH seq={1}>Properties</ArangoTH>
            <ArangoTH seq={1}>Root Analyzers</ArangoTH>
            <ArangoTH seq={3}>Action</ArangoTH>
          </tr>
        </thead>

        <tbody>
          {links &&
            map(linksArr, (p, key) => (
              <Link
                name={p.name}
                analyzers={p.link.analyzers}
                includeAllFields={p.link.includeAllFields}
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
                      onClick={() => viewLink(p.name)}
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
              <i className={`fa ${icon}`} onClick={addClick} />
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
