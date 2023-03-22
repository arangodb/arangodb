declare global {
    interface Window {
        arangoHelper: { [key: string]: any };
        frontendConfig: { [key: string]: any };
    }
}
  
export {};