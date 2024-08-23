import { Box, Stack } from "@chakra-ui/react";
import React from "react";
import { useEditViewContext } from "../../editView/EditViewContext";

export const LinksBreadCrumb = () => {
  const { currentLink, setCurrentLink, setCurrentField, currentField } =
    useEditViewContext();
  return (
    <Stack direction="row" height="10" backgroundColor="gray.200" padding="2">
      <BreadcrumbLink
        onClick={() => {
          setCurrentLink(undefined);
        }}
      >
        Links
      </BreadcrumbLink>
      <Box>/</Box>
      <BreadcrumbLink
        active={currentField.length > 0}
        onClick={() => {
          setCurrentField([]);
        }}
      >
        {currentLink}
      </BreadcrumbLink>
      {currentField.map((field, index) => {
        return (
          <React.Fragment key={index}>
            <Box>/</Box>
            <BreadcrumbLink
              active={index !== currentField.length - 1}
              onClick={() => {
                const newCurrentField = currentField.slice(0, index + 1);
                setCurrentField(newCurrentField);
              }}
            >
              {field}
            </BreadcrumbLink>
          </React.Fragment>
        );
      })}
    </Stack>
  );
};

const BreadcrumbLink = ({
  children,
  onClick,
  active = true
}: {
  children: React.ReactNode;
  onClick: () => void;
  active?: boolean;
}) => {
  return (
    <Box
      as="span"
      color={active ? "blue.500" : ""}
      textDecoration={active ? "underline" : ""}
      cursor={active ? "pointer" : ""}
      onClick={active ? onClick : undefined}
    >
      {children}
    </Box>
  );
};
