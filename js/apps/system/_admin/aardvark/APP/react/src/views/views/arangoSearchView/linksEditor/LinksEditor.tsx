import {
  Box,
  Checkbox,
  FormLabel,
  Grid,
  HStack,
  Stack
} from "@chakra-ui/react";
import { useField } from "formik";
import React, { useEffect, useState } from "react";
import { components, MultiValueGenericProps } from "react-select";
import { MultiSelectControl } from "../../../../components/form/MultiSelectControl";
import CreatableMultiSelect from "../../../../components/select/CreatableMultiSelect";
import MultiSelect from "../../../../components/select/MultiSelect";
import { OptionType } from "../../../../components/select/SelectBase";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { getCurrentDB } from "../../../../utils/arangoClient";
import { useEditViewContext } from "../../editView/EditViewContext";

export const LinksEditor = () => {
  return (
    <>
      <LinksDropdown />
      <LinksDetails />
    </>
  );
};

const LinksDetails = () => {
  const { currentLink } = useEditViewContext();
  if (!currentLink) {
    return null;
  }
  return (
    <Stack backgroundColor="gray.100">
      <LinksBreadCrumb />
      <Grid templateColumns="minmax(300px, 1fr) 1fr" columnGap="10">
        <Stack padding="2">
          <AnalyzersDropdown />
          <FieldsDropdown />
        </Stack>
        <Stack padding="2">
          <HStack>
            <Checkbox
              margin="0"
              borderColor="gray.400"
              id="includeAllFields"
              onChange={() => {
                console.log("onChange");
              }}
            >
              Include All Fields
            </Checkbox>
            <InfoTooltip label="Process all document attributes." />
          </HStack>
        </Stack>
      </Grid>
    </Stack>
  );
};
const MultiValueLabelLinks = (props: MultiValueGenericProps<OptionType>) => {
  const { setCurrentLink } = useEditViewContext();
  return (
    <Box
      style={{
        textDecoration: "underline",
        minWidth: 0 // because parent is flex
      }}
      cursor="pointer"
      onClick={() => {
        setCurrentLink(props.data.value);
      }}
    >
      <components.MultiValueLabel {...props} />
    </Box>
  );
};
const MultiValueLabelFields = (props: MultiValueGenericProps<OptionType>) => {
  const { setCurrentField, currentField } = useEditViewContext();
  return (
    <Box
      style={{
        textDecoration: "underline",
        minWidth: 0 // because parent is flex
      }}
      cursor="pointer"
      onClick={() => {
        setCurrentField([...currentField, props.data.value]);
      }}
    >
      <components.MultiValueLabel {...props} />
    </Box>
  );
};

const LinksDropdown = () => {
  const [options, setOptions] = useState<{ label: string; value: string }[]>();
  const [linksField, , helpers] = useField("links");
  useEffect(() => {
    const db = getCurrentDB();
    const setCollections = async () => {
      const collections = await db.listCollections();
      setOptions(
        collections.map(collection => ({
          label: collection.name,
          value: collection.name
        }))
      );
    };
    setCollections();
  }, []);
  const value = Object.keys(linksField.value).map(key => {
    return {
      label: key,
      value: key
    };
  });
  const addLink = (link: string) => {
    helpers.setValue({
      ...linksField.value,
      [link]: {}
    });
  };
  const removeLink = (link: string) => {
    const newLinks = { ...linksField.value };
    delete newLinks[link];
    helpers.setValue(newLinks);
  };
  return (
    <MultiSelect
      openMenuOnFocus={false}
      openMenuOnClick={false}
      placeholder="Enter a collection name"
      noOptionsMessage={() => "No collections found"}
      components={{
        MultiValueLabel: MultiValueLabelLinks
      }}
      isClearable={false}
      options={options}
      value={value}
      onChange={(_, action) => {
        if (action.action === "remove-value") {
          removeLink(action.removedValue.value);
          return;
        }
        if (action.action === "select-option" && action.option?.value) {
          addLink(action.option.value);
        }
      }}
    />
  );
};
const LinksBreadCrumb = () => {
  const { currentLink, setCurrentLink, currentField } = useEditViewContext();
  return (
    <Stack direction="row" height="10" backgroundColor="gray.200" padding="2">
      <Box
        as="span"
        color="blue.500"
        textDecoration="underline"
        cursor="pointer"
        onClick={() => {
          setCurrentLink(undefined);
        }}
      >
        Links
      </Box>
      <Box>/</Box>
      <Box>{currentLink}</Box>
      {currentField.map((field, index) => {
        return (
          <React.Fragment key={index}>
            <Box>/</Box>
            <Box>{field}</Box>
          </React.Fragment>
        );
      })}
    </Stack>
  );
};
const AnalyzersDropdown = () => {
  const { currentLink } = useEditViewContext();
  return (
    <MultiSelectControl
      name={`links.${currentLink}.analyzers`}
      label="Analyzers"
    />
  );
};
const FieldsDropdown = () => {
  const [fieldsField, , fieldsHelpers] = useField(
    `links.${useEditViewContext().currentLink}.fields`
  );
  const value = Object.keys(fieldsField.value).map(key => {
    return {
      label: key,
      value: key
    };
  });
  const addField = (field: string) => {
    fieldsHelpers.setValue({
      ...fieldsField.value,
      [field]: {}
    });
  };
  const removeField = (field: string) => {
    const newFields = { ...fieldsField.value };
    delete newFields[field];
    fieldsHelpers.setValue(newFields);
  };
  return (
    <>
      <Stack direction="row" alignItems="center">
        <FormLabel margin="0" htmlFor="fields">
          Fields
        </FormLabel>
        <InfoTooltip label="Add field names that you want to be indexed here. Click on a field name to set field details." />
      </Stack>
      <CreatableMultiSelect
        inputId="fields"
        openMenuOnFocus={false}
        openMenuOnClick={false}
        placeholder="Start typing to add a field"
        noOptionsMessage={() => null}
        components={{
          MultiValueLabel: MultiValueLabelFields
        }}
        isClearable={false}
        value={value}
        onChange={(_, action) => {
          if (action.action === "remove-value") {
            removeField(action.removedValue.value);
            return;
          }
          if (action.action === "select-option" && action.option?.value) {
            addField(action.option.value);
          }
        }}
      />
    </>
  );
};
