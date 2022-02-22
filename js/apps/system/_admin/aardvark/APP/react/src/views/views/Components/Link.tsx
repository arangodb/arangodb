import React from "react";
import { ArangoTD } from "../../../components/arango/table";

interface LinkProps {
  link: {
    attr: string;
    name: string;
    desc: string;
    action: React.ReactElement;
    key: number;
  };
}

const Link: React.FC<LinkProps["link"]> = ({
  attr,
  name,
  desc,
  action,
  key
}) => {
  return (
    <tr>
      <ArangoTD seq={key}>{key}</ArangoTD>
      <ArangoTD seq={key}>{attr}</ArangoTD>
      <ArangoTD seq={key}>{name}</ArangoTD>
      <ArangoTD seq={key}>{desc}</ArangoTD>
      <ArangoTD seq={key}>{action}</ArangoTD>
    </tr>
  );
};

export default Link;
