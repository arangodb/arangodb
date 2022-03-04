import React from "react";
import { ArangoTD } from "../../../components/arango/table";
import Badge from "../../../components/arango/badges";

interface LinkProps {
  link: {
    name: string;
    includeAllFields: boolean;
    analyzers: [];
    action: React.ReactElement;
    linkKey: number;
  };
}

const Link: React.FC<LinkProps["link"]> = ({
  name,
  includeAllFields,
  analyzers,
  action,
  linkKey
}) => {
  return (
    <tr>
      <ArangoTD seq={linkKey}>{name}</ArangoTD>
      <ArangoTD seq={linkKey}>{includeAllFields}</ArangoTD>
      <ArangoTD seq={linkKey}>
        {analyzers.map(a => (
          <Badge key={a} name={a} />
        ))}
      </ArangoTD>
      <ArangoTD seq={linkKey}>{action}</ArangoTD>
    </tr>
  );
};

export default Link;
