"""Project settings tools: read/write INI settings, get engine version info."""

from _common import _exec_ue


def register(mcp, send_fn):


    @mcp.tool()
    def get_project_setting(section: str, key: str,
                            ini_file: str = "DefaultEngine") -> str:
        """Read a project setting from an INI config file.
        section: INI section like '/Script/Engine.RendererSettings' or 'URL'.
        key: the setting key within the section.
        ini_file: which config file to read — 'DefaultEngine', 'DefaultGame', or 'DefaultEditor'. Defaults to 'DefaultEngine'."""
        safe_section = section.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        safe_key = key.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        safe_ini = ini_file.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        return _exec_ue(f"""
import os, configparser

project_dir = str(unreal.Paths.project_dir())
ini_name = "{safe_ini}"
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
        section = "{safe_section}"
        key = "{safe_key}"
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
            print(f"\\nFile: {{ini_path}}")
""")


    @mcp.tool()
    def set_project_setting(section: str, key: str, value: str,
                            ini_file: str = "DefaultEngine") -> str:
        """Write a project setting to an INI config file.
        section: INI section like '/Script/Engine.RendererSettings'.
        key: the setting key.
        value: the new value to set.
        ini_file: which config file — 'DefaultEngine', 'DefaultGame', or 'DefaultEditor'.
        Warning: Changes require editor restart to take effect for most settings."""
        safe_section = section.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        safe_key = key.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        safe_value = value.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        safe_ini = ini_file.replace("\\", "\\\\").replace("'", "\\'").replace('"', '\\"')
        return _exec_ue(f"""
import os, configparser

project_dir = str(unreal.Paths.project_dir())
ini_name = "{safe_ini}"
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
        section = "{safe_section}"
        key = "{safe_key}"
        value = "{safe_value}"

        if not config.has_section(section):
            config.add_section(section)

        old_val = config.get(section, key, fallback=None)
        config.set(section, key, value)

        try:
            with open(ini_path, 'w', encoding='utf-8') as f:
                config.write(f)
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
