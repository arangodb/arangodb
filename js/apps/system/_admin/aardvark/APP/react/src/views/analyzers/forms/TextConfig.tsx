import { Grid, Text } from "@chakra-ui/react";
import React from "react";
import { CreatableMultiSelectControl } from "../../../components/form/CreatableMultiSelectControl";
import { InputControl } from "../../../components/form/InputControl";
import { SwitchControl } from "../../../components/form/SwitchControl";
import { AccentInput } from "./inputs/AccentInput";
import { CaseInput } from "./inputs/CaseInput";
import { LocaleInput } from "./inputs/LocaleInput";
import { NGramInputs } from "./inputs/NGramInputs";

export const TextConfig = () => {
  return (
    <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4" rowGap="4">
      <LocaleInput />
      <InputControl name="properties.stopwordsPath" label={"Stopwords Path"} />
      <CreatableMultiSelectControl
        name="properties.stopwords"
        label={"Stopwords"}
      />
      <CaseInput />
      <SwitchControl name="properties.stemming" label={"Stemming"} />
      <AccentInput />
      <Text gridColumn={"1 / -1"} fontWeight="semibold" fontSize="lg">
        Edge N-Gram
      </Text>
      <NGramInputs basePath="properties.edgeNgram" />
    </Grid>
  );
};
