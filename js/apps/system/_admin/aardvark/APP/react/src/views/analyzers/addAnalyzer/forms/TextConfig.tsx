import { Grid, Text } from "@chakra-ui/react";
import React from "react";
import { CreatableMultiSelectControl } from "../../../../components/form/CreatableMultiSelectControl";
import { InputControl } from "../../../../components/form/InputControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";
import { useAnalyzersContext } from "../../AnalyzersContext";
import { AccentInput } from "./inputs/AccentInput";
import { CaseInput } from "./inputs/CaseInput";
import { LocaleInput } from "./inputs/LocaleInput";
import { NGramInputs } from "./inputs/NGramInputs";

export const TextConfig = ({
  basePropertiesPath
}: {
  basePropertiesPath: string;
}) => {
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <LocaleInput basePropertiesPath={basePropertiesPath} />
      <InputControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.stopwordsPath`}
        label={"Stopwords Path"}
      />
      <CreatableMultiSelectControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.stopwords`}
        label={"Stopwords"}
      />
      <CaseInput basePropertiesPath={basePropertiesPath} />
      <SwitchControl
        isDisabled={isDisabled}
        name={`${basePropertiesPath}.stemming`}
        label={"Stemming"}
      />
      <AccentInput basePropertiesPath={basePropertiesPath} />
      <Text gridColumn={"1 / -1"} fontWeight="semibold" fontSize="lg">
        Edge N-Gram
      </Text>
      <NGramInputs basePath={`${basePropertiesPath}.edgeNgram`} />
    </Grid>
  );
};
