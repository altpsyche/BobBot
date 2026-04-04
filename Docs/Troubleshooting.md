# Troubleshooting

## BobBot won't start

**"PythonScriptPlugin not enabled"**
Go to Edit > Plugins, search "Python Script", enable it, restart the editor.

**"Claude Code not found"**
Install Claude Code: `npm install -g @anthropic-ai/claude-code`. Make sure `claude --version` works in a terminal.

**"Not authenticated"**
Run `claude login` in a terminal and complete the sign-in flow.

## Bridge not connecting

**Check bridge status**
Open the Connect tab. The Bridge section shows "Running on port 13580" (green) or "Stopped" (red).

**Restart the bridge**
Click the **Restart** button in Connect > Bridge.

**Port conflict**
If something else is using port 13580, either kill that process or change the bridge port in Connect > Advanced > Bridge > Port.

To find what's using the port: open a terminal and run `netstat -ano | findstr 13580`.

The Connect tab's Advanced > Troubleshooting section has a **Kill Port Conflicts** button that handles this automatically.

## Tools failing

**"Error: _common not initialized"**
The MCP bridge didn't start properly. Restart the bridge from the Connect tab.

**Python errors in the output log**
Open the editor's Output Log (Window > Developer Tools > Output Log) and filter for "LogPython". This shows what Python code BobBot is running and any errors.

**Rebuild the virtual environment**
If tools suddenly stop working, the venv may be corrupted. Go to Connect > Advanced > Troubleshooting > **Rebuild Venv**. This deletes the Python environment and recreates it from scratch.

**Reinstall packages**
If only specific tools fail, try Connect > Advanced > Troubleshooting > **Reinstall Packages**. This re-runs pip install without rebuilding the entire venv.

## Authentication issues

**OAuth not working**
Run `claude login` in a terminal. If you're behind a corporate proxy, you may need to set `HTTP_PROXY` / `HTTPS_PROXY` environment variables.

**API key not accepted**
Make sure you're selecting the right provider (Anthropic, Bedrock, or Vertex) in Connect > Authentication. For Bedrock, you also need an AWS region. For Vertex, you need a GCP project ID and region.

API keys are stored in your OS keychain. If the key was saved but isn't working, try re-entering it.

## Cost budget hit

The default budget is $5.00 per message. If BobBot stops mid-response with a cost error, increase the budget in Connect > Advanced or set it to 0 (unlimited).

The chat header shows a running cost counter. Green means well under budget, yellow means approaching, red means near the limit.

## Run a health check

Connect > Advanced > Troubleshooting > **Run Health Check** tests everything:

- Python venv exists and has pip
- Bridge process is alive on the right port
- Claude Code CLI is found and authenticated
- Agent SDK is importable
- MCP config file is valid

The results appear in a popup window. Each check shows a green "OK" or red "FAIL" with details.

## Factory reset

If nothing else works: Connect > Advanced > Troubleshooting > **Factory Reset**.

This deletes the Python venv, clears all temp files, and resets the setup flag. The next time you open BobBot, the Welcome tab will run fresh setup from scratch. Your conversations and settings are preserved.

## Where logs live

| Log | Location |
|-----|----------|
| Editor output log | `Saved/Logs/` (latest `.log` file) |
| Bridge process log | `Saved/BobBot/_bridge.log` |
| BobBot config | `Saved/Config/BobBot.ini` |
| Python venv | `Saved/BobBot/.venv/` |
| Conversation data | Managed by Claude Code SDK |
