import { FormLabel, Input, Spacer } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { useUrlParameterContext } from "../UrlParametersContext";

export const ParameterEdgeColorComponent = ({
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
      <FormLabel htmlFor="edgeColor">Default edge color</FormLabel>
      <Input
        id="edgeColor"
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
const ParameterEdgeColor = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      edgeColor: event.target.value.replace("#", "")
    };
    setUrlParams(newUrlParameters);
  };
  let calculatedEdgeColor = urlParams.edgeColor;
  if (!calculatedEdgeColor.startsWith("#")) {
    calculatedEdgeColor = "#" + calculatedEdgeColor;
  }

  return (
    <ParameterEdgeColorComponent
      value={calculatedEdgeColor}
      onChange={handleChange}
      isDisabled={urlParams.edgeColorByCollection}
    />
  );
};

export default ParameterEdgeColor;
