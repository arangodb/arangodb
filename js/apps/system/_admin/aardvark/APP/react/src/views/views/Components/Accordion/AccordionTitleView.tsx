import React from "react";
import { IAccordionTitleViewArgs } from "./types";

const AccordionTitleView = ({
  active,
  index,
  testID,
  label,
  onClick
}: IAccordionTitleViewArgs) => {
  //{active ? <b className='caret rotate-180' /> : <b className='caret' />}
  return (
    <span data-testid={testID}>
      <div
        className={`title custom-content ${active ? "active" : ""}`}
        onClick={() => {
          onClick && onClick(index);
        }}
      >
        <div className="accordion-flex" data-testid={`${testID}Flex`}>
          <span data-testid={`${testID}TitleLabel`}>{label}</span>
          {active ? <i className='fa fa-caret-down' /> : <i className='fa fa-caret-up' />}
        </div>
      </div>
    </span>
  );
};

export default AccordionTitleView;
