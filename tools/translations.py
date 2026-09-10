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
out_path = ""


def wl(str):
    print(str)
    out.write(str)
    out.write("\n")


for file in in_dir.glob("*.tr"):
    module_name = file.name[:-3]
    out_path = str(out_dir) + "/" + module_name + ".h"
    print("handling", file, "writing to", out_path)

    entry_key = ""
    entry = ""

    lc = -1
    for raw_line in file.open(encoding="utf-8"):
        line = raw_line.rstrip("\n")
        lc += 1
        if len(line) > 0 and line[0] == "@":
            entry_key = line[1:]
            entry = module_name + "_" + entry_key
            ok = bool(re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", entry_key))
            if ok:
                entries[entry] = []
                continue
            else:
                print(
                    f"line {lc} ERROR: allowed only alphabetical symbols and underscore, got '{entry_key}'"
                )
                sys.exit(-1)
        elif len(line) > 4 and line[2:4] == ": ":
            lang = line[0:2]
            if not (lang.isalpha() and lang.islower()):
                print(
                    f"line {lc} ERROR: expected lowercase language, got '{lang}'",
                    lang,
                )
                sys.exit(-1)

            translation = line[4:]
            print(f"{entry} TR for {lang}: {translation}")
            if languages.get(lang):
                languages[lang] += 1
            else:
                languages[lang] = 1

            entries[entry].append((lang, translation))

        elif len(line) != 1 and line[0] == "\n":
            print(f"line {lc} ERROR: bad line '{line[:-1]}'")
            sys.exit(-1)
        else:
            print("empty")

    print(entries)
    print(languages)

    print("now writing...")
    out_dir.mkdir(parents=True, exist_ok=True)
    out = open(out_path, "w", encoding="utf-8")

    wl("#pragma once\n")
    wl('#include "langs.h"\n')

    wl(f"static {translation_type} trs[{len(languages)}] = ")
    wl("{")
    i = -1
    for lang in languages:
        wl("{")
        i += 1
        j = -1
        for key in entries.values():
            j += 1
            found = next((t for t in key if t[0] == lang), None)
            translation = found[1] if found != None else key[0][1]
            comma = "" if j == len(entries) - 1 else ","
            wl("\t" + str_literal(translation) + comma)
        comma = "" if i == len(languages) - 1 else ","
        wl("}" + comma)

    wl("};")

    print("lc", lc)

out_path = str(out_dir) + "/langs.h"
out = open(out_path, "w", encoding="utf-8")


wl("#pragma once\n")
wl('#include "base/str_view.h"\n')
wl("enum Lang : int8_t {")
for lang in languages:
    wl(f"\tlang_{lang},")
wl("\tlang_COUNT")
wl("};\n")

wl(f"struct {translation_type} {{")
i = 0
for key in entries:
    i += 1
    wl(f"\t{str_type} {key};")
wl("};\n")

wl(f"inline {str_type} lang_code(Lang lang){{")
wl("\tswitch(lang) {")
for lang in languages:
    wl(f'\t case lang_{lang}:\n\t\treturn "{lang}"_v;')
wl('\t default:\n\t\treturn "UNKNOWN"_v;')
wl("}\n}\n")
