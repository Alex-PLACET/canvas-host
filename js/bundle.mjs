/**
 * Concatenates the compiled runtime modules into the single embedded runtime bundle.
 */

import { readFile, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const inputDir = resolve(process.argv[2] ?? join(here, "build"));
const outputFile = resolve(process.argv[3] ?? join(inputDir, "canvas-host.js"));
const modules = ["constants", "utils", "events", "state", "attach", "main"];

let bundle = "(() => {\n";
for (const name of modules) {
    bundle += await readFile(join(inputDir, `${name}.js`), "utf8");
    bundle += "\n";
}
bundle += "})();\n";

await writeFile(outputFile, bundle);
