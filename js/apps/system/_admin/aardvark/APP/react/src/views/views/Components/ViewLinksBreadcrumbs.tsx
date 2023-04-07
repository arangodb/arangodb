import { Box, HStack, Text } from "@chakra-ui/react";
import React from "react";
import { useLinksContext } from "../LinksContext";

/**
 * this function extracts the data between square brackets
 * for eg
 * input: links["a"]
 * output: ["links", "a"]
 */

const extractDataBetweenSquareBrackets = (path: string) => {
  let result = [];
  let firstPart = path.split("[")[0];
  let secondPart = path.split("[")[1];
  secondPart = secondPart.split("]")[0];
  // remove the quotes
  secondPart = secondPart.split('"')[1];

  result = [firstPart, secondPart];
  return result;
};

/**
 * input: links["a"].fields["b"].fields["c"]
 * output: [['links', 'a'], ['fields', 'b], ['fields', 'c']]
 */
const getFragments = (currentPath?: string) => {
  if (!currentPath) {
    return [];
  }
  const pathParts = currentPath.split(".");
  const fragments = pathParts.map(part => {
    return extractDataBetweenSquareBrackets(part);
  });
  return fragments;
};

/**
 * this function creates a path from the fragments
 * input: [['links', 'a'], ['fields', 'b], ['fields', 'c']]
 * output: links["a"].fields["b"].fields["c"]

 *  */
const getPathFromFragments = (fragments: string[][]) => {
  let path = "";
  fragments.forEach((fragment, index) => {
    if (index === 0) {
      path = fragment[0] + '["' + fragment[1] + '"]';
    } else {
      path = path + "." + fragment[0] + '["' + fragment[1] + '"]';
    }
  });
  return path;
};

export const ViewLinksBreadcrumbs = () => {
  const { setCurrentField, currentField } = useLinksContext();
  const fragments = getFragments(currentField?.fieldPath);
  return (
    <Box fontSize="md">
      <Box backgroundColor="gray.800" paddingX="4" paddingY="2">
        <HStack listStyleType="none" as="ul" color="gray.100" flexWrap="wrap">
          <Box
            textDecoration="underline"
            _hover={{ color: "gray.100" }}
            cursor="pointer"
            onClick={() => setCurrentField(undefined)}
          >
            Links
          </Box>
          <Box>{">>"}</Box>
          {fragments
            .slice(0, fragments.length - 1)
            .map(([_path, fragment], idx) => {
              return (
                <HStack as="li" key={`${idx}-${fragment}`}>
                  <Box
                    textDecoration="underline"
                    _hover={{ color: "gray.100" }}
                    cursor="pointer"
                    isTruncated
                    maxWidth="200px"
                    title={fragment}
                    onClick={() => {
                      const currentFragments = fragments.slice(0, idx + 1);
                      const path = getPathFromFragments(currentFragments);
                      path &&
                        setCurrentField({
                          fieldPath: path
                        });
                    }}
                  >
                    {fragment}
                  </Box>
                  <Box>{">>"}</Box>
                </HStack>
              );
            })}
          <Text
            isTruncated
            maxWidth="200px"
            title={fragments[fragments.length - 1]?.[1]}
          >
            {fragments[fragments.length - 1]?.[1]}
          </Text>
        </HStack>
      </Box>
    </Box>
  );
};
