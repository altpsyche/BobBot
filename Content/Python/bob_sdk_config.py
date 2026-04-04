"""
BobBot SDK — Configuration & system prompt.

Holds project root resolution, system prompt, model state,
MCP server config, agent definitions, and internal MCP server.
"""

import os
import sys
import json
import bob_platform


# --------------------------------------------------------------------------- #
# Logging
# --------------------------------------------------------------------------- #
def _log_sdk(msg):
    try:
        import unreal
        unreal.log(msg)
    except Exception:
        print(msg, file=sys.stderr)


# --------------------------------------------------------------------------- #
# Project root & working directory
# --------------------------------------------------------------------------- #
def _resolve_project_root():
    """Get the absolute project root directory."""
    try:
        import unreal
        uproject = str(unreal.Paths.get_project_file_path())
        if uproject:
            root = os.path.abspath(os.path.dirname(os.path.normpath(uproject)))
            if os.path.isdir(root):
                return root
    except Exception:
        pass
    env_root = os.environ.get("BOB_PROJECT_ROOT", "")
    if env_root:
        return os.path.normpath(env_root)
    return os.getcwd()


_PROJECT_ROOT = _resolve_project_root()
_BOB_CWD = os.path.join(_PROJECT_ROOT, "Saved", "BobBot")
os.makedirs(_BOB_CWD, exist_ok=True)

try:
    import unreal as _unreal_init
    _unreal_init.log("BobBot SDK chat: PROJECT_ROOT = {}".format(_PROJECT_ROOT))
except Exception:
    pass


# --------------------------------------------------------------------------- #
# System prompt
# --------------------------------------------------------------------------- #
_SYSTEM_PROMPT = (
    "You are BobBot, an AI assistant embedded inside the Unreal Engine 5 editor. "
    "You help users with Unreal Engine tasks: creating assets, editing Blueprints, "
    "modifying levels, writing gameplay code, and answering questions about their project.\n\n"
    "You have MCP tools connected to the running UE editor. Prefer them over execute_unreal_python.\n\n"
    "Speak in plain, conversational language. Strictly avoid emojis, dashes, Unicode symbols, "
    "bullet points, or structured formatting. Do not use predictable AI phrasing or robotic "
    "enthusiasm. Keep your responses direct, natural, and human.\n\n"
    "Be concise. Say what you did and the result. No filler."
)

_SYSTEM_PROMPT_PATH = os.path.join(_PROJECT_ROOT, "Saved", "BobBot", "_system_prompt.txt")


def _get_system_prompt():
    """Get the active system prompt (custom override or inlined default)."""
    if os.path.isfile(_SYSTEM_PROMPT_PATH):
        try:
            with open(_SYSTEM_PROMPT_PATH, "r", encoding="utf-8") as f:
                content = f.read().strip()
            if content:
                return content
        except Exception:
            pass
    return _SYSTEM_PROMPT


def get_default_prompt():
    """Return the built-in default system prompt string."""
    return _SYSTEM_PROMPT


# --------------------------------------------------------------------------- #
# Model state
# --------------------------------------------------------------------------- #
_model = "sonnet"


def get_model():
    return _model


def set_model(name):
    """Set model. If client is connected, switch live via SDK control protocol."""
    global _model
    if name not in ("sonnet", "opus", "haiku"):
        return
    _model = name
    # Live switch is handled by bob_chat_sdk facade (needs client + loop access)


# --------------------------------------------------------------------------- #
# MCP server config
# --------------------------------------------------------------------------- #
def _build_mcp_servers():
    """Build MCP server config dict for ClaudeAgentOptions."""
    saved_dir = os.path.join(_PROJECT_ROOT, "Saved", "BobBot")

    # Try HTTP config first (persistent bridge)
    http_config = os.path.join(saved_dir, "_bobbot_mcp.json")
    if os.path.isfile(http_config):
        try:
            with open(http_config, "r") as f:
                cfg = json.load(f)
            servers = cfg.get("mcpServers", {})
            unreal = servers.get("unreal", {})
            if unreal.get("type") == "http":
                try:
                    import bob_bridge_launcher
                    if bob_bridge_launcher.is_running():
                        return {"unreal": unreal}
                except Exception:
                    pass
        except Exception:
            pass

    # Fallback to stdio config
    fallback = os.path.join(saved_dir, "_bobbot_mcp_fallback.json")
    config_path = fallback if os.path.isfile(fallback) else os.path.join(
        _PROJECT_ROOT, ".mcp.json")
    if os.path.isfile(config_path):
        try:
            with open(config_path, "r") as f:
                cfg = json.load(f)
            servers = cfg.get("mcpServers", {})
            if "unreal" in servers:
                return {"unreal": servers["unreal"]}
        except Exception:
            pass

    return {}


# --------------------------------------------------------------------------- #
# Custom agent definitions
# --------------------------------------------------------------------------- #
def _load_agent_definitions():
    """Load custom agent definitions from Saved/BobBot/agents.json."""
    agents_file = os.path.join(_BOB_CWD, "agents.json")
    if not os.path.isfile(agents_file):
        return None
    try:
        with open(agents_file, "r", encoding="utf-8") as f:
            raw = json.load(f)
        from claude_agent_sdk import AgentDefinition
        agents = {}
        for name, defn in raw.items():
            agents[name] = AgentDefinition(
                description=defn["description"],
                prompt=defn["prompt"],
                tools=defn.get("tools"),
                model=defn.get("model"),
                maxTurns=defn.get("maxTurns"),
            )
        return agents
    except Exception as e:
        _log_sdk("BobBot SDK: failed to load agents.json: {}".format(e))
        return None


# --------------------------------------------------------------------------- #
# In-process MCP tools (zero latency, no TCP round-trip)
# --------------------------------------------------------------------------- #
_bobbot_internal_server = None


def _get_internal_mcp_server(get_status_fn):
    """Create in-process MCP server with BobBot utility tools.

    Args:
        get_status_fn: callable that returns the status dict (avoids circular import).
    """
    global _bobbot_internal_server
    if _bobbot_internal_server is not None:
        return _bobbot_internal_server
    try:
        # Ensure pywin32 DLLs are registered before SDK imports that need them
        try:
            import bob_bridge_launcher
            sp = bob_bridge_launcher.get_venv_site_packages()
            if sp:
                bob_platform.register_pywin32_dlls(sp)
        except Exception:
            pass
        from claude_agent_sdk import create_sdk_mcp_server, tool

        @tool(name="ping_unreal",
              description="Check if UE editor is responsive",
              input_schema={})
        async def ping_tool(params):
            try:
                import unreal
                return {"content": [{"type": "text", "text": "pong"}]}
            except Exception:
                return {"content": [{"type": "text", "text": "Editor not responding"}],
                        "is_error": True}

        @tool(name="get_bobbot_status",
              description="Get BobBot diagnostic info (backend, model, session, context)",
              input_schema={})
        async def status_tool(params):
            s = get_status_fn()
            return {"content": [{"type": "text", "text": json.dumps(s, indent=2)}]}

        _bobbot_internal_server = create_sdk_mcp_server(
            "bobbot-internal", "1.0", tools=[ping_tool, status_tool])
        return _bobbot_internal_server
    except Exception as e:
        _log_sdk("BobBot SDK: failed to create internal MCP server: {}".format(e))
        return None


# --------------------------------------------------------------------------- #
# Environment-based SDK options
# --------------------------------------------------------------------------- #
def get_sdk_options_from_env():
    """Read all BOB_* env vars and return a dict of SDK configuration values."""
    opts = {}

    # Permission mode
    bob_perm = os.environ.get("BOB_PERMISSION_MODE", "edit_automatically")
    if bob_perm == "ask_before_edits":
        opts["permission_mode"] = "acceptEdits"
    elif bob_perm == "plan":
        opts["permission_mode"] = "plan"
    else:
        opts["permission_mode"] = "bypassPermissions"
    opts["bob_permission_mode"] = bob_perm

    # Thinking config
    thinking_mode = os.environ.get("BOB_THINKING_MODE", "disabled")
    if thinking_mode == "enabled":
        budget = 10000
        try:
            budget = int(os.environ.get("BOB_THINKING_BUDGET", "10000"))
        except (ValueError, TypeError):
            pass
        opts["thinking"] = {"type": "enabled", "budget_tokens": budget}
    elif thinking_mode == "adaptive":
        opts["thinking"] = {"type": "adaptive"}

    # Effort control
    effort = os.environ.get("BOB_EFFORT", "")
    if effort in ("low", "medium", "high", "max"):
        opts["effort"] = effort

    # Cost budget
    max_budget = os.environ.get("BOB_MAX_BUDGET", "")
    if max_budget:
        try:
            opts["max_budget_usd"] = float(max_budget)
        except (ValueError, TypeError):
            pass

    # Fallback model
    fallback = os.environ.get("BOB_FALLBACK_MODEL", "")
    if fallback:
        opts["fallback_model"] = fallback

    # Disallowed tools
    blocked = os.environ.get("BOB_DISALLOWED_TOOLS", "")
    if blocked:
        opts["disallowed_tools"] = [t.strip() for t in blocked.split(",") if t.strip()]

    # Additional directories
    extra_dirs = os.environ.get("BOB_ADD_DIRS", "")
    if extra_dirs:
        opts["extra_add_dirs"] = [d.strip() for d in extra_dirs.split(";") if d.strip()]

    # Beta features (1M context)
    if os.environ.get("BOB_BETA_1M_CONTEXT") == "1":
        opts["betas"] = ["interleaved-thinking", "extended-context"]

    # Task budget (token-paced execution)
    task_budget_str = os.environ.get("BOB_TASK_BUDGET", "")
    if task_budget_str:
        try:
            opts["task_budget"] = {"total": int(task_budget_str)}
        except (ValueError, TypeError):
            pass

    # Max turns safety limit
    max_turns_str = os.environ.get("BOB_MAX_TURNS", "")
    if max_turns_str:
        try:
            opts["max_turns"] = int(max_turns_str)
        except (ValueError, TypeError):
            pass

    return opts
