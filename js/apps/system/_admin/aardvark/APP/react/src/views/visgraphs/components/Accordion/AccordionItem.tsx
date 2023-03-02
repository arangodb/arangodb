import React from "react";
import AccordionTitleView from "./AccordionTitleView";
import { IAccordionItemViewArgs } from "./types";

const AccordionItem = (accordionItem: IAccordionItemViewArgs) => {
  const { active, content, testID } = accordionItem;
  return (
    <>
      <AccordionTitleView {...accordionItem} />

      {active && (
        <div
          className="content accordion-content"
          data-testid={`${testID}Content`}
        >
          {content}
        </div>
      )}
    </>
  );
};

export default AccordionItem;