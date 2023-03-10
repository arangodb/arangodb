import { ReactNode } from "react";

interface IAccordionItem {
  active?: boolean;
  index: number;
  label: string;
  testID?: string;
  defaultActive?: boolean;
}

export interface IAccordionItemViewArgs extends IAccordionItem {
  content: ReactNode;
  clickHandler?: (index: number) => void;
  allowMultipleOpen?: boolean;
}

export interface IAccordionViewArgs {
  accordionConfig: Array<IAccordionItemViewArgs>;
  allowMultipleOpen?: boolean;
}

export interface IAccordionTitleViewArgs extends IAccordionItem {
  className?: string;
  clickHandler?: (index: number) => void;
  allowMultipleOpen?: boolean;
}