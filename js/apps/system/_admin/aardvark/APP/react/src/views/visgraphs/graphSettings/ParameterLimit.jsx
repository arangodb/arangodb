import { FormLabel, Input } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterLimit = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [limit, setLimit] = useState(urlParams.limit);

  const handleChange = event => {
    setLimit(event.target.value);
    const newUrlParameters = { ...urlParams, limit: event.target.value };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="limit">Limit</FormLabel>
      <Input
        id="limit"
        width="60px"
        min={1}
        required={true}
        value={limit}
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
