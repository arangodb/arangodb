import React, { ReactNode, useContext } from "react";
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
import { useJsonFormEffect } from "../helpers";

const ViewLayout = ({ view, field, link, children }: ViewLayoutProps) => {
  const { setShow } = useContext(ViewContext);
  const [json, toggleButton] = useJsonFormEffect(false);

  const handeleJsonClick = () => {
    toggleButton();
    setShow("JsonView");
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
              icon={!json ? "toggle-off" : "toggle-on"}
              buttonName={!json ? "Json View" : "Form view"}
              buttonClick={handeleJsonClick}
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
