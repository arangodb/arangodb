import { Database } from "arangojs";

async function globalTeardown() {
  console.log("global teardown");
  console.log("...");
  const db = new Database({
    url: "http://localhost:3001",
    auth: { username: "root", password: "" },
  });
  // purge the database
  db.dropDatabase("test");
}

export default globalTeardown;
