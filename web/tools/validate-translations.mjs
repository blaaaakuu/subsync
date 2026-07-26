import { readdirSync, readFileSync } from "node:fs";
import { dirname, extname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const webRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const requested = process.argv.slice(2);
const paths = requested.length
  ? requested.map((path) => resolve(webRoot, path))
  : readdirSync(resolve(webRoot, "src", "translations"))
      .filter((name) => extname(name) === ".json")
      .map((name) => resolve(webRoot, "src", "translations", name));

let invalid = false;
for (const path of paths) {
  const translations = JSON.parse(readFileSync(path, "utf8"));
  for (const [source, translation] of Object.entries(translations)) {
    if (!source || typeof translation !== "string" || !translation.trim()) {
      console.error(`${path}: invalid translation for ${JSON.stringify(source)}`);
      invalid = true;
    }
  }
}

if (invalid) {
  process.exitCode = 1;
} else {
  console.log(`Validated ${paths.length} translation file(s).`);
}
