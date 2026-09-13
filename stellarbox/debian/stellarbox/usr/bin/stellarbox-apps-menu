#!/usr/bin/python3
#
# stellarbox-apps-menu - genera el menú de Aplicaciones de Stellarbox
# (Openbox pipe menu) a partir de los .desktop instalados en el sistema.
#
# Se ejecuta cada vez que se abre el menú, por lo que refleja de inmediato
# las aplicaciones recién instaladas (y quita las desinstaladas).
#
# Uso: /usr/bin/stellarbox-apps-menu
# Salida: XML openbox pipe menu (raíz <openbox_pipe_menu>) por stdout.

import os
import re
import shlex
import shutil
import sys

# ----------------------------------------------------------------------------
# categorías .desktop -> submenú, en orden de prioridad
CATEGORY_MAP = [
    ("System",        ("Settings", "System", "HardwareSettings",
                       "Administration")),
    ("Multimedia",    ("AudioVideo", "Audio", "Video", "Player")),
    ("Development",   ("Development", "IDE")),
    ("Education",     ("Education",)),
    ("Games",         ("Game",)),
    ("Graphics",      ("Graphics",)),
    ("Internet",      ("Network", "WebBrowser", "InstantMessaging", "Email",
                       "IRCClient", "Chat")),
    ("Office",        ("Office", "Finance", "PIM", "TextEditor")),
    ("Science",       ("Science", "Math")),
    ("Accessories",   ("Accessories", "Utility", "ConsoleOnly")),
]

CATEGORY_ORDER = [cat for cat, _ in CATEGORY_MAP]

# terminales probadas para aplicaciones con Terminal=true
TERMINALS = ["x-terminal-emulator", "konsole", "xfce4-terminal",
             "gnome-terminal", "urxvt", "xterm"]


def xml_escape(text):
    return (text.replace("&", "&amp;").replace("<", "&lt;")
                .replace(">", "&gt;").replace('"', "&quot;"))


def parse_desktop(path):
    """Devuelve un dict key/value de la primera sección del .desktop."""
    data = {}
    try:
        with open(path, "r", errors="replace") as fh:
            for raw in fh:
                line = raw.strip()
                if not line or line.startswith("#") or line.startswith("["):
                    continue
                if "=" not in line:
                    continue
                key, _, value = line.partition("=")
                data[key.strip()] = value.strip()
    except OSError:
        return None
    return data


def desktop_name(data, locales):
    for loc in locales:
        if loc is None:
            if "Name" in data:
                return data["Name"]
        else:
            value = data.get("Name[%s]" % loc)
            if value:
                return value
    return data.get("Name")


def primary_category(categories):
    for cat, names in CATEGORY_MAP:
        for name in names:
            if name in categories:
                return cat
    return "Other"


def find_terminal():
    for term in TERMINALS:
        if shutil.which(term):
            return term
    return "xterm"


def build_menu():
    home = os.path.expanduser("~")

    # directorios de aplicaciones, en orden de precedencia
    dirs = [os.path.join(home, ".local/share/applications"),
            "/usr/local/share/applications",
            "/usr/share/applications"]
    xdg_data = os.environ.get("XDG_DATA_DIRS")
    if xdg_data:
        dirs.extend(os.path.join(d, "applications")
                    for d in reversed(xdg_data.split(":")))

    # locale para nombres localizados
    locales = []
    lang = os.environ.get("LANG") or "C"
    if lang and lang != "C":
        locales.append(lang.split(".")[0])
        family = locales[-1].split("_")[0]
        if family != locales[-1]:
            locales.append(family)
    locales.append(None)  # Name (sin sufijo)

    terminal = None
    seen = {}  # nombre de archivo -> ruta (gana el de mayor precedencia)
    for d in dirs:
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".desktop"):
                continue
            seen.setdefault(fn, os.path.join(d, fn))

    apps = {}  # categoría -> [(nombre, comando)]
    for path in seen.values():
        data = parse_desktop(path)
        if not data:
            continue
        if data.get("Hidden") == "true":
            continue
        if "Type" in data and data["Type"] != "Application":
            continue
        if data.get("NoDisplay") == "true":
            continue
        name = desktop_name(data, locales)
        if not name:
            continue
        cmd = data.get("Exec", "")
        if not cmd:
            continue
        cmd = re.sub(r"%(.)", r"", cmd).strip()
        if not cmd:
            continue
        if data.get("Terminal") == "true":
            if terminal is None:
                terminal = find_terminal()
            cmd = terminal + " -e sh -c " + shlex.quote(cmd)
        cats = [c.strip() for c in data.get("Categories", "").split(";")
                if c.strip()]
        cat = primary_category(cats)
        apps.setdefault(cat, []).append((name, cmd))

    # salida: pipe menu
    out = []
    out.append("<openbox_pipe_menu>")
    if apps:
        for cat in CATEGORY_ORDER:
            if cat not in apps:
                continue
            entries = sorted(apps[cat], key=lambda e: e[0].lower())
            if len(entries) == 1 and cat == "Other":
                item = entries[0]
                out.append('  <item label="%s">' % xml_escape(item[0]))
                out.append("    <action name=\"Execute\">"
                           "<command>%s</command></action>"
                           % xml_escape(item[1]))
                out.append("  </item>")
            else:
                out.append('  <menu id="category-%s" label="%s">'
                           % (xml_escape(cat), xml_escape(cat)))
                for item in entries:
                    out.append('    <item label="%s">' % xml_escape(item[0]))
                    out.append("      <action name=\"Execute\">"
                               "<command>%s</command></action>"
                               % xml_escape(item[1]))
                    out.append("    </item>")
                out.append("  </menu>")
        other = apps.get("Other", [])
        if other:
            entries = sorted(other, key=lambda e: e[0].lower())
            out.append('  <menu id="category-Other" label="Other">')
            for item in entries:
                out.append('    <item label="%s">' % xml_escape(item[0]))
                out.append("      <action name=\"Execute\">"
                           "<command>%s</command></action>"
                           % xml_escape(item[1]))
                out.append("    </item>")
            out.append("  </menu>")
    else:
        out.append("  <item label=\"(no applications found)\">")
        out.append("    <action name=\"Execute\">"
                   "<command>true</command></action>")
        out.append("  </item>")
    out.append("</openbox_pipe_menu>")
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    build_menu()