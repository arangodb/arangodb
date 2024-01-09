import { Box } from "@chakra-ui/react";
import { useField } from "formik";
import React, { useEffect, useState } from "react";
import { components, MultiValueGenericProps } from "react-select";
import MultiSelect from "../../../../components/select/MultiSelect";
import { OptionType } from "../../../../components/select/SelectBase";
import { getCurrentDB } from "../../../../utils/arangoClient";
import { useEditViewContext } from "../../editView/EditViewContext";

const MultiValueLabelLinks = (props: MultiValueGenericProps<OptionType>) => {
  const { setCurrentLink, setCurrentField } = useEditViewContext();
  return (
    <Box
      style={{
        textDecoration: "underline",
        minWidth: 0 // because parent is flex
      }}
      cursor="pointer"
      onClick={() => {
        setCurrentLink(props.data.value);
        setCurrentField([]);
      }}
    >
      <components.MultiValueLabel {...props} />
    </Box>
  );
};
export const LinksDropdown = () => {
  const [options, setOptions] = useState<{ label: string; value: string }[]>();
  const [linksField, , helpers] = useField("links");
  const { isAdminUser } = useEditViewContext();
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
  const value = Object.keys(linksField.value)
    .map(key => {
      if (linksField.value[key] === null) {
        return null;
      }
      return {
        label: key,
        value: key
      };
    })
    .filter(Boolean) as OptionType[];
  const addLink = (link: string) => {
    helpers.setValue({
      ...linksField.value,
      [link]: {}
    });
  };
  const removeLink = (link: string) => {
    const newLinks = { ...linksField.value, [link]: null };
    helpers.setValue(newLinks);
  };
  return (
    <MultiSelect
      openMenuOnFocus={false}
      openMenuOnClick={false}
      placeholder="Enter a collection name"
      noOptionsMessage={() => "No collections found"}
      isDisabled={!isAdminUser}
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
