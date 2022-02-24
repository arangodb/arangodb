import React from "react";
import { ArangoTD } from "../../../components/arango/table";

interface LinkProps {
  link: {
    name: string;
    properties: boolean[];
    action: React.ReactElement;
    key: number;
  };
}

const Link: React.FC<LinkProps["link"]> = ({
  name,
  properties,
  action,
  key
}) => {
  return (
    <tr>
      <ArangoTD seq={key}>{name}</ArangoTD>
      <ArangoTD seq={key}>{properties}</ArangoTD>
      <ArangoTD seq={key}>{action}</ArangoTD>
    </tr>
  );
};

export default Link;
