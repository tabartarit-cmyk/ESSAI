#!/usr/bin/env python3
"""Outil de traduction de l'interface.

  python3 tools/i18n_tool.py extract   Régénère translations/_template.json à partir des tr("...") du code
  python3 tools/i18n_tool.py check     Vérifie chaque translations/<code>.json (textes manquants, marqueurs %1, ;;, retours à la ligne)

Chaque langue est un fichier translations/<code>.json (code ISO 639-1) de la forme
{ "texte anglais exact": "traduction" }. Ajouter une langue = ajouter un fichier.
"""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCES = ["picoashamainwindow.cpp", "remotedevice.cpp", "pairdialog.cpp"]
STR = r'"(?:[^"\\\n]|\\.)*"'
CALL = re.compile(r'\btr\(\s*((?:' + STR + r')(?:\s*(?:' + STR + r'))*)\s*[,)]')
PART = re.compile(STR)


def unescape(lit: str) -> str:
    body = lit[1:-1]
    out, i = [], 0
    while i < len(body):
        ch = body[i]
        if ch == "\\" and i + 1 < len(body):
            n = body[i + 1]
            out.append({"n": "\n", "t": "\t", '"': '"', "'": "'", "\\": "\\"}.get(n, n))
            i += 2
        else:
            out.append(ch)
            i += 1
    return "".join(out)


def extract_keys():
    keys = []
    for name in SOURCES:
        text = (ROOT / name).read_text(encoding="utf-8").replace("\r\n", "\n")
        for m in CALL.finditer(text):
            key = "".join(unescape(p) for p in PART.findall(m.group(1)))
            if key and key not in keys:
                keys.append(key)
    return keys


def write_json(path: Path, data: dict):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def cmd_extract():
    keys = extract_keys()
    write_json(ROOT / "translations" / "_template.json", {k: "" for k in keys})
    print(f"{len(keys)} textes -> translations/_template.json")


def markers(s):
    return sorted(re.findall(r"%\d", s))


def cmd_check():
    keys = extract_keys()
    problems = 0
    for f in sorted((ROOT / "translations").glob("*.json")):
        if f.name.startswith("_"):
            continue
        data = json.loads(f.read_text(encoding="utf-8"))
        missing = [k for k in keys if not data.get(k)]
        unknown = [k for k in data if k not in keys and not k.startswith("@")]
        bad = []
        for k in keys:
            v = data.get(k)
            if not v:
                continue
            if markers(k) and markers(k) != markers(v):
                bad.append((k, "marqueurs %n différents"))
            elif k.count(";;") != v.count(";;"):
                bad.append((k, "séparateur ;; différent"))
            elif k.count("\n") != v.count("\n"):
                bad.append((k, "retours à la ligne différents (info)"))
        status = "OK" if not (missing or unknown or bad) else "À VÉRIFIER"
        print(f"{f.stem:6s} {len(keys)-len(missing):3d}/{len(keys)}  {status}")
        for k in missing[:5]:
            print("    manquant :", k[:60].replace("\n", " "))
        for k in unknown[:5]:
            print("    inconnu  :", k[:60].replace("\n", " "))
        for k, why in bad[:5]:
            print("    ", why, ":", k[:50].replace("\n", " "))
        problems += len(missing) + len(unknown) + sum(1 for _, w in bad if "info" not in w)
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    {"extract": cmd_extract, "check": cmd_check}.get(sys.argv[1] if len(sys.argv) > 1 else "", lambda: print(__doc__))()
