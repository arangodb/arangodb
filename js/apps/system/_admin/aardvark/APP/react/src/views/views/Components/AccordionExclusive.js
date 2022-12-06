import React from "react";
import 'semantic-ui-css/semantic.min.css';
import { Accordion, Form } from "semantic-ui-react";

const AccordionExclusive = ({panels}) => (
  <Accordion
    defaultActiveIndex={[0, 2]}
    panels={panels}
    exclusive={false}
    fluid
  />
);

export default AccordionExclusive;
