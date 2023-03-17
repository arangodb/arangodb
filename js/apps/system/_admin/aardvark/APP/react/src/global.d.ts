declare global {
    interface Window {
        arangoHelper: { [key: string]: any };
    }
}
  
export {};