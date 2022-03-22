import React from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { useShow, useShowUpdate } from "../Contexts/LinkContext";

type ViewLayoutProps = {
  view?: string | undefined;
  field?: string;
  link?: string;
};

const ViewLayout: React.FC<ViewLayoutProps> = ({
  view,
  field,
  link,
  children
}) => {
  const show = useShow();
  const updateShow = useShowUpdate();

  const handleLinkClick = (
    e: React.MouseEvent<HTMLAnchorElement, MouseEvent>,
    str: string | undefined
  ) => {
    e.preventDefault();
    updateShow("ViewParent");
    console.log(str);
    console.log(show);
  };
  return (
    <ArangoTable>
      <thead>
        <tr>
          <ArangoTH seq={0} style={{ width: "100%" }}>
            <a href={`/${view}`}>{view}</a>
            <a href={`/${link}`} onClick={e => handleLinkClick(e, link)}>
              {link !== undefined ? "/" + link : ""}
            </a>
            <a href={`/${field}`}>{field !== undefined ? "/" + field : ""}</a>
          </ArangoTH>
        </tr>
      </thead>
      <tbody><tr><ArangoTD seq={0}>{children}</ArangoTD></tr></tbody>
    </ArangoTable>
  );
};

export default ViewLayout;
