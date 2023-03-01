import React, { useEffect, useState } from "react";
import AccordionItem from "./AccordionItem";
import { IAccordionItemViewArgs, IAccordionViewArgs } from "./types";
import "./styles.css";

const AccordionView = ({
  accordionConfig = [],
  allowMultipleOpen
}: IAccordionViewArgs) => {
  const [activeIndices, setActiveIndices] = useState<Array<number>>([]);

  useEffect(() => {
    const defaultActiveTab = accordionConfig.filter(
      (config) => config.defaultActive
    );
    if (defaultActiveTab.length) {
      setActiveIndices([defaultActiveTab[0].index]);
    }
  }, []);

  const handleClick = (index: number) => {
    const doesIndexExist = !!activeIndices.filter((val) => val === index)
      .length;

    if (allowMultipleOpen) {
      if (doesIndexExist) {
        const updatedIndices = activeIndices.filter((val) => val !== index);
        setActiveIndices([...updatedIndices]);
      } else {
        setActiveIndices([...activeIndices, index]);
      }
    } else {
      if (doesIndexExist) {
        setActiveIndices([]);
      } else {
        setActiveIndices([index]);
      }
    }
  };

  if (!accordionConfig || !accordionConfig.length) {
    return null;
  }
  return (
    <div className="ui styled accordion">
      {accordionConfig.map((config: IAccordionItemViewArgs) => {
        return (
          <AccordionItem
            {...config}
            allowMultipleOpen={allowMultipleOpen}
            key={config.index}
            active={activeIndices.includes(config.index)}
            clickHandler={handleClick}
            testID={`accordion${config.index}`}
          />
        );
      })}
    </div>
  );
};

export default AccordionView;