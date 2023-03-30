declare global {
  interface Window {
    arangoHelper: { [key: string]: any };
    frontendConfig: { [key: string]: any };
    App: any;
  }
}

export {};