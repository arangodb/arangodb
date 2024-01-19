import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeLabelByCollection = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      nodeLabelByCollection: event.target.checked
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeLabelByCollection">
        Show collection name
      </FormLabel>
      <Checkbox
        id="nodeLabelByCollection"
        isChecked={urlParams.nodeLabelByCollection}
        onChange={handleChange}
      />
      <InfoTooltip label={"Adds a collection name to the node label."} />
    </>
  );
};

export default ParameterNodeLabelByCollection;
