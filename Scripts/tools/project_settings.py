"""Project settings tools: read/write INI settings, get engine version info."""

from _common import _exec_ue, _safe


def register(mcp, send_fn):


    @mcp.tool()
    def get_project_setting(section: str, key: str,
                            ini_file: str = "DefaultEngine") -> str:
        """Read a project setting from an INI config file.
        section: INI section like '/Script/Engine.RendererSettings' or 'URL'.
        key: the setting key within the section.
        ini_file: which config file to read — 'DefaultEngine', 'DefaultGame', or 'DefaultEditor'. Defaults to 'DefaultEngine'."""
        return _exec_ue(f"""
import os, configparser

project_dir = str(unreal.Paths.project_dir())
ini_name = {_safe(ini_file)}
if not ini_name.endswith(".ini"):
    ini_name += ".ini"
ini_path = os.path.join(project_dir, "Config", ini_name)

if not os.path.isfile(ini_path):
    print(f"ERROR: Config file not found: {{ini_path}}")
else:
    config = configparser.ConfigParser(strict=False)
    config.optionxform = str  # Preserve case
    try:
        config.read(ini_path, encoding='utf-8')
    except Exception as e:
        print(f"ERROR: Failed to parse {{ini_name}}: {{e}}")
    else:
        section = {_safe(section)}
        key = {_safe(key)}
        if not config.has_section(section):
            # List available sections to help the user
            sections = config.sections()
            print(f"ERROR: Section '{{section}}' not found in {{ini_name}}")
            if sections:
                print(f"Available sections ({{len(sections)}}):")
                for s in sections[:20]:
                    print(f"  {{s}}")
                if len(sections) > 20:
                    print(f"  ... and {{len(sections) - 20}} more")
        elif not config.has_option(section, key):
            # List keys in the section
            keys = config.options(section)
            print(f"ERROR: Key '{{key}}' not found in [{{section}}]")
            if keys:
                print(f"Available keys ({{len(keys)}}):")
                for k in keys[:20]:
                    print(f"  {{k}} = {{config.get(section, k)}}")
                if len(keys) > 20:
                    print(f"  ... and {{len(keys) - 20}} more")
        else:
            val = config.get(section, key)
            print(f"[{{section}}] {{key}} = {{val}}")
            print()
            print(f"File: {{ini_path}}")
""")


    @mcp.tool()
    def set_project_setting(section: str, key: str, value: str,
                            ini_file: str = "DefaultEngine") -> str:
        """Write a project setting to an INI config file.
        section: INI section like '/Script/Engine.RendererSettings'.
        key: the setting key.
        value: the new value to set.
        ini_file: which config file — 'DefaultEngine', 'DefaultGame', or 'DefaultEditor'.
        Warning: Changes require editor restart to take effect for most settings.

        Uses line-level editing to preserve UE INI features like +/- prefixes,
        duplicate keys, and comments."""
        return _exec_ue(f"""
import os

project_dir = str(unreal.Paths.project_dir())
ini_name = {_safe(ini_file)}
if not ini_name.endswith(".ini"):
    ini_name += ".ini"
ini_path = os.path.join(project_dir, "Config", ini_name)

if not os.path.isfile(ini_path):
    print(f"ERROR: Config file not found: {{ini_path}}")
else:
    section = {_safe(section)}
    key = {_safe(key)}
    value = {_safe(value)}

    with open(ini_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Find the section and key using line-level parsing
    section_header = "[" + section + "]"
    section_start = -1
    section_end = len(lines)
    in_section = False
    key_line = -1
    old_val = None

    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith('[') and ']' in stripped:
            if in_section:
                # Hit the next section — stop searching
                section_end = i
                break
            if stripped == section_header:
                section_start = i
                in_section = True
        elif in_section and '=' in stripped and not stripped.startswith(';') and not stripped.startswith('#'):
            line_key = stripped.split('=', 1)[0].strip()
            if line_key == key:
                key_line = i
                old_val = stripped.split('=', 1)[1].strip()

    if section_start < 0:
        # Section not found — append new section and key
        lines.append("\\n" + section_header + "\\n")
        lines.append(key + "=" + value + "\\n")
    elif key_line < 0:
        # Section found but key not found — insert after section header
        lines.insert(section_start + 1, key + "=" + value + "\\n")
    else:
        # Replace existing key value, keeping any prefix on the key
        original_line = lines[key_line]
        eq_pos = original_line.index('=')
        prefix = original_line[:eq_pos + 1]
        lines[key_line] = prefix + value + "\\n"

    try:
        with open(ini_path, 'w', encoding='utf-8') as f:
            f.writelines(lines)
        if old_val is not None:
            print(f"Updated [{{section}}] {{key}}: {{old_val}} -> {{value}}")
        else:
            print(f"Added [{{section}}] {{key}} = {{value}}")
        print(f"File: {{ini_path}}")
        print("Note: Most settings require an editor restart to take effect.")
    except Exception as e:
        print(f"ERROR: Failed to write {{ini_name}}: {{e}}")
""")


    @mcp.tool()
    def get_engine_version() -> str:
        """Get the Unreal Engine version, Python version, and OS platform info."""
        return _exec_ue("""
import sys, platform

engine_ver = unreal.SystemLibrary.get_engine_version()
print(f"Unreal Engine: {engine_ver}")
print(f"Python: {sys.version}")
print(f"Platform: {platform.system()} {platform.release()} ({platform.machine()})")
print(f"OS: {platform.platform()}")
""")
