import React, { ReactNode, useContext, useState } from "react";
import {
  ArangoTable,
  ArangoTD,
  ArangoTH
} from "../../../components/arango/table";
import { ViewContext } from "../ViewLinksReactView";
import { JsonButton } from "../Actions";
type ViewLayoutProps = {
  view?: string | undefined;
  field?: string;
  link?: string;
  children: ReactNode;
};

const ViewLayout = ({ view, field, link, children }: ViewLayoutProps) => {
  const { setShow } = useContext(ViewContext);
  const [json, setJson] = useState(false);

  const handleJsonClick = () => {
    setJson(!json);
  };
  return (
    <ArangoTable>
      <thead>
        <tr>
          <ArangoTH seq={0} style={{ width: "100%" }}>
            <a href={`/${view}`}>{view}</a>
            <a href={`/${link}`} onClick={() => setShow("ViewParent")}>
              {link !== undefined ? "/" + link : ""}
            </a>
            <a href={`/${field}`}>{field !== undefined ? "/" + field : ""}</a>
            <JsonButton
              icon={!json ? "code" : "align-right"}
              buttonName={!json ? "Json View" : "Form view"}
              buttonClick={handleJsonClick}
            />
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

export default ViewLayout;
