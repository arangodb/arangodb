export const useCustomHashMatch = (pattern: string) => {
  const split = window.location.hash.split(pattern);
  return split && split[1];
};
