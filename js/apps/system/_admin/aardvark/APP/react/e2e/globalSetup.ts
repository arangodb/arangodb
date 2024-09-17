import { Database } from "arangojs";

async function globalSetup() {
  const db = new Database({
    url: "http://localhost:3001",
    auth: { username: "root", password: "" }
  });
  db.createDatabase("test");
  console.log("global setup");
  console.log("...");
}

export default globalSetup;
