import React from "react";
import { ChildProp, Int } from "../../utils/constants";

export const ArangoTable = ({ children }: ChildProp) => <table className={'arango-table'}>{children}</table>;

type ArangoTHProps = ChildProp & {
  seq: Int;
  [key: string]: any
};

export const ArangoTH = ({children, seq, ...rest}: ArangoTHProps) =>
  <th className={`arango-table-th table-cell${seq}`} {...rest}>{children}</th>;

export const ArangoTD = ({children, seq, ...rest}: ArangoTHProps) =>
  <td className={`arango-table-td table-cell${seq}`} {...rest}>{children}</td>;
