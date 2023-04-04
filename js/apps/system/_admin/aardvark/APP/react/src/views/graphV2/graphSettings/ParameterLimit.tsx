import { FormLabel, Input } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterLimit = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    if (Number(event.target.value) >= 0) {
      const newUrlParameters = {
        ...urlParams,
        limit: Number(event.target.value)
      };
      setUrlParams(newUrlParameters);
    }
  };

  return (
    <>
      <FormLabel htmlFor="limit">Limit</FormLabel>
      <Input
        id="limit"
        width="60px"
        value={urlParams.limit}
        type="number"
        size="sm"
        onChange={handleChange}
      />
      <InfoTooltip
        label={"Limit nodes count. If empty or zero, no limit is set."}
      />
    </>
  );
};

export default ParameterLimit;
