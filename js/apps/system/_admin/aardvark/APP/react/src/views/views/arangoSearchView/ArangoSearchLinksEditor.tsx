import { useField } from "formik";
import React, { useEffect, useState } from "react";
import MultiSelect from "../../../components/select/MultiSelect";
import { getCurrentDB } from "../../../utils/arangoClient";

export const ArangoSearchLinksEditor = () => {
  return (
    <>
      <LinksDropdown />
    </>
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
