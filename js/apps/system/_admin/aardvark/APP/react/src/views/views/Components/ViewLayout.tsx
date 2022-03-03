import React from "react";
import { ArangoTable, ArangoTH } from "../../../components/arango/table";

type ViewLayoutProps = {
  disabled: boolean | undefined;
  view?: string | undefined;
  field?: string;
  link?: string;
};

const ViewLayout: React.FC<ViewLayoutProps> = ({
  disabled,
  view,
  field,
  link,
  children
}) => {
  return (
    <ArangoTable>
      <thead>
        <tr>
          <ArangoTH seq={disabled ? 0 : 1} style={{ width: "100%" }}>
            <a href={`/${view}`}>{view}</a>/<a href={`/${link}`}>{link}</a>/
            <a href={`/${field}`}>{field}</a>
          </ArangoTH>
        </tr>
      </thead>
      <tbody>{children}</tbody>
    </ArangoTable>
  );
};

export default ViewLayout;
