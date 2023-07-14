export type CreateDatabaseUserValues = {
  role: string;
  name: string;
  extra: {
    img: string;
    name: string;
  };
  active: boolean;
  user: string;
  passwd?: string;
};

export type DatabaseUserValues = {
  extra: {
    img: string;
    name: string;
  };
  user: string;
  active: boolean;
  name: string;
  passwd?: string;
};
