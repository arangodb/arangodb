import { ReactNode } from "react";

export type ChildProp = {
  children?: ReactNode;
};

export type Int = number & { __int__: void };
