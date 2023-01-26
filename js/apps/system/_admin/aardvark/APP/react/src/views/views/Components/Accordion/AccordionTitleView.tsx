import React from "react";
import { IAccordionTitleViewArgs } from "./types";

const AccordionTitleView = ({
  active,
  index,
  testID,
  label,
  clickHandler
}: IAccordionTitleViewArgs) => {
  return (
    <span data-testid={testID}>
      <div
        className={`title custom-content ${active ? "active" : ""}`}
        onClick={() => {
          clickHandler && clickHandler(index);
        }}
      >
        <div className="accordion-flex" data-testid={`${testID}Flex`}>
          <span data-testid={`${testID}TitleLabel`}>{label}</span>
          {active ? <i className='fa fa-caret-up' /> : <i className='fa fa-caret-down' />}
        </div>
      </div>
    </span>
  );
};

export default AccordionTitleView;
