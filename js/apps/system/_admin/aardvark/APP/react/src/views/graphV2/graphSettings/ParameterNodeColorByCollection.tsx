import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

export const ParameterNodeColorByCollectionComponent = ({
  isChecked,
  onChange
}: {
  isChecked: boolean;
  onChange: (event: ChangeEvent<HTMLInputElement>) => void;
}) => {
  return (
    <>
      <FormLabel htmlFor="nodeColorByCollection">
        Color nodes by collection
      </FormLabel>
      <Checkbox
        id="nodeColorByCollection"
        isChecked={isChecked}
        onChange={onChange}
      />
      <InfoTooltip
        label={
          "Should nodes be colorized by their collection? If enabled, node color and node color attribute will be ignored."
        }
      />
    </>
  );
};
const ParameterNodeColorByCollection = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      nodeColorByCollection: event.target.checked
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <ParameterNodeColorByCollectionComponent
      isChecked={urlParams.nodeColorByCollection}
      onChange={handleChange}
    />
  );
};

export default ParameterNodeColorByCollection;
