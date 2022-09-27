import React, { CSSProperties } from "react";
import { ChildProp } from "../../utils/constants";

type ArangoTableProps = ChildProp & {
  [key: string]: any;
};

export const ArangoTable = ({ children, ...rest }: ArangoTableProps) => {
  const style: CSSProperties = Object.assign({ border: "1px solid #e5e5e5" }, rest.style || {});

  return (
    <table
      className={"arango-table"}
      {...rest}
      style={style}
    >
      {children}
    </table>
  );
};

type ArangoTHProps = ChildProp & {
  seq: number;
  [key: string]: any;
};

export const ArangoTH = ({ children, seq, ...rest }: ArangoTHProps) => (
  <th className={`arango-table-th table-cell${seq}`} {...rest}>
    {children}
  </th>
);

export const ArangoTD = ({ children, seq, ...rest }: ArangoTHProps) => (
  <td className={`arango-table-td table-cell${seq}`} {...rest}>
    {children}
  </td>
);
