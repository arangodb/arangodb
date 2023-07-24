import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterEdgeColorByCollection = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      edgeColorByCollection: event.target.checked
    };
    setUrlParams(newUrlParameters);
  };
  return (
    <>
      <FormLabel htmlFor="edgeColorByCollection">
        Color edges by collection
      </FormLabel>
      <Checkbox
        id="edgeColorByCollection"
        isChecked={urlParams.edgeColorByCollection}
        onChange={handleChange}
      />
      <InfoTooltip
        label={
          "Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored."
        }
      />
    </>
  );
};

export default ParameterEdgeColorByCollection;
