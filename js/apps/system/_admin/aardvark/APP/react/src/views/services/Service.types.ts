import { ServiceSummary } from "arangojs/database";

export type ServiceDescription = ServiceSummary & {
  system: boolean;
  config: {[key: string]: any};
  deps: {[key: string]: any};
};
