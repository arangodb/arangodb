import { Box } from "@chakra-ui/react";
import React from "react";
import { FilterOptionOption } from "react-select/dist/declarations/src/filters";
import { Modal } from "../../../components/modal";
import { OptionType } from "../../../components/select/SelectBase";
import SingleSelect from "../../../components/select/SingleSelect";
import { useQueryContext } from "../QueryContextProvider";
import { useQuerySpotlightOptions } from "./useQuerySpotlightOptions";

export const QuerySpotlight = () => {
  const { isSpotlightOpen, onCloseSpotlight } = useQueryContext();
  return (
    <Modal
      size="2xl"
      onClose={onCloseSpotlight}
      isOpen={isSpotlightOpen}
      modalContentProps={{
        backgroundColor: "transparent",
        marginTop: "120px"
      }}
    >
      <Box>
        <SpotlightSelect />
      </Box>
    </Modal>
  );
};

const SpotlightSelect = () => {
  const { groupedOptions, isLoading } = useQuerySpotlightOptions();
  const { aqlJsonEditorRef, onCloseSpotlight } = useQueryContext();
  const filterOption = (
    { label, value }: FilterOptionOption<OptionType>,
    searchString: string
  ) => {
    // default search
    if (
      label.toLocaleLowerCase().includes(searchString) ||
      value.toLocaleLowerCase().includes(searchString)
    ) {
      return true;
    }

    // check if a group as the filter searchString as label
    const groupOptions = groupedOptions.filter(group =>
      group.label.toLocaleLowerCase().includes(searchString)
    );

    if (groupOptions) {
      for (const groupOption of groupOptions) {
        // Check if current option is in group
        const option = groupOption.options.find(opt => opt.value === value);
        if (option) {
          return true;
        }
      }
    }
    return false;
  };
  if (isLoading) {
    return null;
  }
  return (
    <SingleSelect
      autoFocus
      filterOption={filterOption}
      isClearable={false}
      options={groupedOptions}
      placeholder="Search"
      onChange={newValue => {
        const editor = (aqlJsonEditorRef.current as any)?.jsonEditor;
        const aceEditor = editor.aceEditor;
        aceEditor.insert(newValue?.value);
        aceEditor.focus();
        onCloseSpotlight();
      }}
      components={{
        IndicatorsContainer: () => null
      }}
      noOptionsMessage={() => "No results found"}
      styles={{
        control: base => ({
          ...base,
          backgroundColor: "var(--chakra-colors-gray-800)",
          borderColor: "var(--chakra-colors-gray-900)",
          color: "white"
        }),
        input: base => ({
          ...base,
          color: "white"
        })
      }}
    />
  );
};
