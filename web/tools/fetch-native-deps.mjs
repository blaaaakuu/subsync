import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const webRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const nativeRoot = resolve(webRoot, "native");

const dependencies = [
  {
    name: "ffmpeg",
    repository: "https://github.com/FFmpeg/FFmpeg.git",
    tag: "n6.1.6",
  },
  {
    name: "pocketsphinx",
    repository: "https://github.com/cmusphinx/pocketsphinx.git",
    tag: "v5.1.1",
  },
];

mkdirSync(nativeRoot, { recursive: true });

for (const dependency of dependencies) {
  const target = resolve(nativeRoot, dependency.name);
  if (existsSync(target)) {
    console.log(`${dependency.name}: using existing source at ${target}`);
    continue;
  }

  console.log(`${dependency.name}: cloning ${dependency.tag}`);
  execFileSync(
    "git",
    [
      "clone",
      "--depth",
      "1",
      "--branch",
      dependency.tag,
      dependency.repository,
      target,
    ],
    { stdio: "inherit" },
  );
}
