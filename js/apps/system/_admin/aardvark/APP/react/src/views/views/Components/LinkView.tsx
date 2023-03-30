import React, { useContext } from "react";
import { FormState, ViewContext } from "../constants";
import LinkPropertiesInput from "../forms/inputs/LinkPropertiesInput";
import ViewLinkLayout from "./ViewLinkLayout";

interface LinkViewProps {
  links: FormState["links"];
  disabled: boolean | undefined;
  link: string;
  name: string;
}

const LinkView = ({ links, disabled, link }: LinkViewProps) => {
  const { dispatch } = useContext(ViewContext);

  return links && links[link] ? (
    <ViewLinkLayout fragments={[link]}>
      <LinkPropertiesInput
        formState={links[link] || {}}
        disabled={disabled}
        dispatch={dispatch}
        basePath={`links[${link}]`}
      />
    </ViewLinkLayout>
  ) : null;
};
export default LinkView;
