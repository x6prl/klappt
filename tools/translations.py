#!/usr/bin/python3
import re
import sys
from pathlib import Path

in_dir = Path("translations")
out_dir = Path("src/ui/translations")

str_type = "StrView"
translation_type = "UiTranslation"


def escape_cpp_string(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')


def str_literal(value):
    return '"' + escape_cpp_string(value) + '"_v'


languages = {}
entries = {}

files = sorted(in_dir.glob("*.tr"))
for file in files:
    module_name = file.stem

    for lc, raw_line in enumerate(file.open(encoding="utf-8")):
        line = raw_line.strip()
        if not line:
            continue

        if line.startswith("@"):
            entry_key = line[1:]
            entry = module_name + "_" + entry_key
            if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", entry_key):
                print(
                    f"{file}:{lc} ERROR: allowed only alphabetical symbols and underscore, got '{entry_key}'"
                )
                sys.exit(-1)
            entries[entry] = []
        elif len(line) > 4 and line[2:4] == ": ":
            lang = line[0:2]
            if not (lang.isalpha() and lang.islower()):
                print(f"{file}:{lc} ERROR: expected lowercase language, got '{lang}'")
                sys.exit(-1)

            translation = line[4:]
            languages[lang] = languages.get(lang, 0) + 1
            entries[entry].append((lang, translation))
        else:
            print(f"{file}:{lc} ERROR: bad line '{raw_line.rstrip()}'")
            sys.exit(-1)

out_dir.mkdir(parents=True, exist_ok=True)

langs_path = out_dir / "langs.h"
with open(langs_path, "w", encoding="utf-8") as out:
    out.write("#pragma once\n\n")
    out.write('#include "base/str_view.h"\n\n')
    out.write("enum Lang : int8_t {\n")
    for lang in languages:
        out.write(f"\tlang_{lang},\n")
    out.write("\tlang_COUNT\n};\n\n")

    out.write(f"struct {translation_type} {{\n")
    for key in entries:
        out.write(f"\t{str_type} {key};\n")
    out.write("};\n\n")

    out.write(f"inline {str_type} lang_code(Lang lang){{\n")
    out.write("\tswitch(lang) {\n")
    for lang in languages:
        out.write(f'\t case lang_{lang}:\n\t\treturn "{lang}"_v;\n')
    out.write('\t default:\n\t\treturn "UNKNOWN"_v;\n')
    out.write("}\n}\n")

for file in files:
    module_name = file.stem
    out_path = out_dir / f"{module_name}.h"

    with open(out_path, "w", encoding="utf-8") as out:
        out.write("#pragma once\n\n")
        out.write('#include "langs.h"\n\n')
        out.write(f"static {translation_type} trs[{len(languages)}] = \n{{\n")

        for i, lang in enumerate(languages):
            out.write("{\n")
            for j, key_translations in enumerate(entries.values()):
                found = next((t for t in key_translations if t[0] == lang), None)
                # Fallback to first translation (en) if this lang is missing
                translation = found[1] if found is not None else key_translations[0][1]
                comma = "" if j == len(entries) - 1 else ","
                out.write(f"\t{str_literal(translation)}{comma}\n")

            comma = "" if i == len(languages) - 1 else ","
            out.write("}" + comma + "\n")

        out.write("};\n")
