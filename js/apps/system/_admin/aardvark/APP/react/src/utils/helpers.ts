import { ChangeEvent } from "react";

export const getChangeHandler = (setter: (value: string) => void) => {
  return (event: ChangeEvent<HTMLInputElement | HTMLSelectElement>) => {
    setter(event.currentTarget.value);
  };
};

export const hasOwnProperty = <X extends {}, Y extends PropertyKey> (obj: X, prop: Y): obj is X & Record<Y, unknown> => {
  return obj.hasOwnProperty(prop);
};
