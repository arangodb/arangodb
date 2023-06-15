import { ExternalLinkIcon } from "@chakra-ui/icons";
import {
  Divider,
  Flex,
  Grid,
  Link,
  Stack,
  Switch,
  Text
} from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";
import { SelectControl } from "../../../components/form/SelectControl";
import { AnalyzerTypes } from "../Analyzer.types";
import { useAnalyzersContext } from "../AnalyzersContext";
import { TYPE_TO_LABEL_MAP } from "../AnalyzersHelpers";
import { AnalyzerTypeForm } from "./AnalyzerTypeForm";

const ANALYZER_TYPE_OPTIONS = Object.keys(TYPE_TO_LABEL_MAP).map(type => {
  return {
    value: type,
    label: TYPE_TO_LABEL_MAP[type as AnalyzerTypes]
  };
});

export const AddAnalyzerForm = ({
  initialFocusRef
}: {
  initialFocusRef?: React.RefObject<HTMLInputElement>;
}) => {
  const [analyzerTypeField] = useField<AnalyzerTypes>("type");
  const { isFormDisabled: isDisabled } = useAnalyzersContext();
  const analyzerTypeValue = analyzerTypeField.value;
  return (
    <Grid templateColumns={"1fr 1fr"} gap="6">
      <Stack>
        <Text>Basic</Text>
        <Divider borderColor="gray.400" />
        <Grid templateColumns={"1fr 1fr"} columnGap="4" alignItems="start">
          <InputControl
            ref={initialFocusRef}
            isDisabled={isDisabled}
            name="name"
            label="Analyzer name"
          />
          <Flex alignItems="flex-end">
            <SelectControl
              isDisabled={isDisabled}
              name="type"
              label="Analyzer type"
              selectProps={{
                options: ANALYZER_TYPE_OPTIONS
              }}
            />
            <Link
              spacing="1"
              as={Stack}
              alignItems="center"
              direction="row"
              marginLeft="2"
              marginBottom="2"
              target="_blank"
              href={`https://www.arangodb.com/docs/stable/analyzers.html#${analyzerTypeValue}`}
            >
              <Text>Docs</Text> <ExternalLinkIcon />
            </Link>
          </Flex>
        </Grid>
      </Stack>
      <Stack>
        <Text>Features</Text>
        <Divider borderColor="gray.400" />
        <Grid templateColumns={"1fr 1fr 1fr"} rowGap="4">
          <FeatureSwitch name="frequency" />
          <FeatureSwitch name="norm" />
          <FeatureSwitch name="position" />
        </Grid>
      </Stack>
      <AnalyzerTypeForm analyzerType={analyzerTypeValue} />
    </Grid>
  );
};

const FEATURE_NAME_TO_LABEL_MAP = {
  frequency: "Frequency",
  norm: "Norm",
  position: "Position"
};

const FeatureSwitch = ({ name }: { name: string }) => {
  const [featuresField, , featuresFieldHelper] = useField("features");
  const { isFormDisabled: isDisabled } = useAnalyzersContext();

  return (
    <Switch
      display="flex"
      alignItems="center"
      isDisabled={isDisabled}
      isChecked={featuresField.value.includes(name)}
      onChange={() => {
        if (featuresField.value.includes(name)) {
          featuresFieldHelper.setValue(
            featuresField.value.filter((feature: string) => feature !== name)
          );
          return;
        }
        featuresFieldHelper.setValue([...featuresField.value, name]);
      }}
    >
      {
        FEATURE_NAME_TO_LABEL_MAP[
          name as keyof typeof FEATURE_NAME_TO_LABEL_MAP
        ]
      }
    </Switch>
  );
};
