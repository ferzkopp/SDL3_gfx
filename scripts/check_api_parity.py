#!/usr/bin/env python3
"""Check that the public API of SDL3_gfx matches SDL2_gfx (decision D8 in the modernization plan).

Compares the function prototypes and the macro names declared in the public headers of an SDL2_gfx
checkout and this SDL3_gfx checkout after normalizing the SDL2/SDL3 prefixes. Only the differences
listed in ALLOWED_* are accepted.

    python scripts/check_api_parity.py <SDL2_gfx dir> [<SDL3_gfx dir>]

Exit code 0: same API, 1: differences, 2: usage or file errors.
"""
import pathlib
import re
import sys

MODULES = ["gfxPrimitives", "rotozoom", "framerate", "imageFilter"]

# Functions that exist only in SDL3_gfx (additions from the SDL3 port by sabdul-khabir, Q3).
ALLOWED_ADDED_FUNCTIONS = {"SDL_imageFilterMultUnbound", "SDL_imageFilterMultInv"}
# Macros that may differ (version numbers, export scopes and header guards are not API).
IGNORED_MACRO = re.compile(r"^(SDL[23]_GFXPRIMITIVES_(MAJOR|MINOR|MICRO)|SDL[23]_\w+_SCOPE|_SDL[23]_\w+_h)$")

PROTOTYPE = re.compile(r"SDL[23]_\w+_SCOPE\s+([^;{}()]+?)\s*\b(\w+)\s*\(([^;{}]*?)\)\s*;", re.S)
DEFINE = re.compile(r"^\s*#\s*define\s+(\w+)", re.M)


def normalize(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text.replace("SDL3_", "SDLx_").replace("SDL2_", "SDLx_")


def read_api(directory, version):
    functions, macros = {}, set()
    for module in MODULES:
        path = pathlib.Path(directory) / f"SDL{version}_{module}.h"
        if not path.is_file():
            raise FileNotFoundError(path)
        raw = path.read_text(encoding="utf-8", errors="replace")
        for match in PROTOTYPE.finditer(re.sub(r"/\*.*?\*/|//[^\n]*", " ", raw, flags=re.S)):
            ret, name, params = (" ".join(normalize(part).split()) for part in match.groups())
            params = re.sub(r"\s*([*,()])\s*", r"\1", params)
            functions[name] = (module, f"{ret} {name}({params})")
        for name in DEFINE.findall(raw):
            if not IGNORED_MACRO.match(name):
                macros.add((module, normalize(name)))
    return functions, macros


def main(argv):
    if len(argv) not in (2, 3):
        print(__doc__)
        return 2
    sdl2_dir = argv[1]
    sdl3_dir = argv[2] if len(argv) == 3 else str(pathlib.Path(__file__).resolve().parent.parent)
    try:
        f2, m2 = read_api(sdl2_dir, 2)
        f3, m3 = read_api(sdl3_dir, 3)
    except FileNotFoundError as error:
        print(f"Header not found: {error}")
        return 2

    problems = []
    for name in sorted(set(f2) - set(f3)):
        problems.append(f"missing in SDL3_gfx: {f2[name][1]}")
    for name in sorted(set(f3) - set(f2) - ALLOWED_ADDED_FUNCTIONS):
        problems.append(f"only in SDL3_gfx: {f3[name][1]}")
    for name in sorted(set(f2) & set(f3)):
        if f2[name] != f3[name]:
            problems.append(f"different: SDL2_gfx {f2[name][0]}: {f2[name][1]}\n"
                            f"           SDL3_gfx {f3[name][0]}: {f3[name][1]}")
    for module, name in sorted(m2 ^ m3):
        problems.append(f"macro {name} ({module}.h) only in SDL{2 if (module, name) in m2 else 3}_gfx")

    for problem in problems:
        print(problem)
    print(f"{len(f2)} SDL2_gfx functions, {len(f3)} SDL3_gfx functions, {len(m3)} macros: "
          f"{'OK' if not problems else f'{len(problems)} difference(s)'}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
