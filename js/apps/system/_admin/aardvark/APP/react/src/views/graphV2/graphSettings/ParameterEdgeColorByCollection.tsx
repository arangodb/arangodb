import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

export const ParameterEdgeColorByCollectionComponent = ({
  isChecked,
  onChange
}: {
  isChecked: boolean;
  onChange: (event: ChangeEvent<HTMLInputElement>) => void;
}) => {
  return (
    <>
      <FormLabel htmlFor="edgeColorByCollection">
        Color edges by collection
      </FormLabel>
      <Checkbox
        id="edgeColorByCollection"
        isChecked={isChecked}
        onChange={onChange}
      />
      <InfoTooltip
        label={
          "Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored."
        }
      />
    </>
  );
};
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
    <ParameterEdgeColorByCollectionComponent
      isChecked={urlParams.edgeColorByCollection}
      onChange={handleChange}
    />
  );
};

export default ParameterEdgeColorByCollection;
