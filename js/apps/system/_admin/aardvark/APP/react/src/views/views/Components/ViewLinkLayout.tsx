import React, { ReactNode } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";

type ViewLinkLayoutProps = {
  view?: string | undefined;
  field?: string;
  link?: string;
  children: ReactNode;
};

const ViewLinkLayout = ({ view, field, link, children }: ViewLinkLayoutProps) => {
  return (
    <ArangoTable>
      <thead>
        <tr>
          <ArangoTH seq={0} style={{ width: "100%" }}>
            <a href={`/${view}`}>{view}</a>
            <a href={`/${link}`}>
              {link !== undefined ? "/" + link : ""}
            </a>
            <a href={`/${field}`}>{field !== undefined ? "/" + field : ""}</a>
          </ArangoTH>
        </tr>
      </thead>
      <tbody>
        <tr>
          <ArangoTD seq={0}>{children}</ArangoTD>
        </tr>
      </tbody>
    </ArangoTable>
  );
};

export default ViewLinkLayout;
