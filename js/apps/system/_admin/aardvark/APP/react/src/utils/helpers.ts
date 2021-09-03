import { ChangeEvent } from "react";

export const getChangeHandler = (setter: (value: string) => void) => {
  return (event: ChangeEvent<HTMLInputElement | HTMLSelectElement>) => {
    setter(event.currentTarget.value);
  };
};
