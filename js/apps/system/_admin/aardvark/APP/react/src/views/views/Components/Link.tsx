import React from "react";
import { ArangoTD } from "../../../components/arango/table";

interface LinkProps {
  link: {
    name: string;
    includeAllFields: boolean;
    analyzers: [];
    action: React.ReactElement;
    key: number;
  };
}

const Link: React.FC<LinkProps["link"]> = ({
  name,
  includeAllFields,
  analyzers,
  action,
  key
}) => {
  return (
    <tr>
      <ArangoTD seq={key}>{name}</ArangoTD>
      <ArangoTD seq={key}>{includeAllFields}</ArangoTD>
      <ArangoTD seq={key}>{analyzers}</ArangoTD>
      <ArangoTD seq={key}>{action}</ArangoTD>
    </tr>
  );
};

export default Link;
