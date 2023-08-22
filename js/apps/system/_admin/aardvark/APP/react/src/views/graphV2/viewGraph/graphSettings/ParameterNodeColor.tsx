import { FormLabel, Input, Spacer } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { useUrlParameterContext } from "../UrlParametersContext";

export const ParameterNodeColorComponent = ({
  value,
  onChange,
  isDisabled
}: {
  value: string;
  onChange: (event: ChangeEvent<HTMLInputElement>) => void;
  isDisabled: boolean;
}) => {
  return (
    <>
      <FormLabel htmlFor="nodeColor">Default node color </FormLabel>
      <Input
        id="nodeColor"
        type="color"
        value={value}
        style={{
          width: "60px",
          height: "30px"
        }}
        onChange={onChange}
        disabled={isDisabled}
      />
      <Spacer />
    </>
  );
};
const ParameterNodeColor = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      nodeColor: event.target.value.replace("#", "")
    };
    setUrlParams(newUrlParameters);
  };

  let calculatedNodeColor = urlParams.nodeColor;
  if (!calculatedNodeColor.startsWith("#")) {
    calculatedNodeColor = "#" + calculatedNodeColor;
  }

  return (
    <ParameterNodeColorComponent
      isDisabled={urlParams.nodeColorByCollection}
      value={calculatedNodeColor}
      onChange={handleChange}
    />
  );
};

export default ParameterNodeColor;
