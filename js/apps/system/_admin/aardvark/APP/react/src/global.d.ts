type FrontendConfig = {
  [key: string]: any;
  isCluster: boolean;
  isEnterprise: boolean;
  db: string;
  forceOneShard: boolean;
  ldapEnabled: boolean;
  extendedNames?: boolean;
};

declare global {
  interface Window {
    arangoHelper: { [key: string]: any };
    frontendConfig: FrontendConfig;
    versionHelper: { [key: string]: any };
    arangoValidationHelper: { [key: string]: any };
    App: any;
  }
}

export {};
