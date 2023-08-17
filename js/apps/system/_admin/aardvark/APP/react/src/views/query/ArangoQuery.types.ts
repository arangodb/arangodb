import { CursorExtras } from "arangojs/cursor";


export type QueryResultType<ResultType extends any = any> = {
  type: "query" | "profile" | "explain";
  queryValue: string;
  queryBindParams?: { [key: string]: string; };
  result?: ResultType;
  extra?: CursorExtras;
  status: "success" | "error" | "loading";
  asyncJobId?: string;
  profile?: any;
  stats?: any;
  warnings?: {
    code: number;
    message: string;
  }[];
  errorMessage?: string;
  queryLimit?: number | "all";
};
