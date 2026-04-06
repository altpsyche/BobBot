"""
BobBot Chat — thin re-export layer over bob_chat_sdk.

All chat functionality is in bob_chat_sdk.py (ClaudeSDKClient persistent process).
This module exists so C++ code can `import bob_chat` without changing import paths.
"""

import bob_chat_sdk as _sdk

# Core chat
def send_message(text):    return _sdk.send_message(text)
def poll():                return _sdk.poll()
def cleanup():             return _sdk.cleanup()
def clear_session():       return _sdk.clear_session()
def is_thinking():         return _sdk.is_thinking()
def interrupt():           return _sdk.interrupt()

# Configuration
def set_model(name):       return _sdk.set_model(name)
def get_model():           return _sdk.get_model()
def set_thinking_mode(mode, budget=None): return _sdk.set_thinking_mode(mode, budget)
def set_effort(level):     return _sdk.set_effort(level)
def get_status():          return _sdk.get_status()
def get_session_cost():    return _sdk.get_session_cost()
def get_session_id():      return _sdk.get_session_id()
def set_session_id(sid):   return _sdk.set_session_id(sid)
def get_default_prompt():  return _sdk.get_default_prompt()

# Detection & Auth
def detect_claude():       return _sdk.detect_claude()
def check_auth():          return _sdk.check_auth()
def check_sdk_available(): return _sdk.check_sdk_available()
def ensure_ready():        return _sdk.ensure_ready()

# SDK features (Phase 2)
def set_permission_mode(mode): return _sdk.set_permission_mode(mode)
def stop_task(task_id):    return _sdk.stop_task(task_id)

# Hooks (Phase 3)
def set_permission_decision(request_id, d): return _sdk.set_permission_decision(request_id, d)

# Session management (Phase 4)
def list_saved_sessions(limit=50, offset=0): return _sdk.list_saved_sessions(limit, offset)
def get_saved_session_info(sid): return _sdk.get_saved_session_info(sid)
def get_saved_session_messages(sid, limit=100, offset=0): return _sdk.get_saved_session_messages(sid, limit, offset)
def rename_saved_session(sid, title): return _sdk.rename_saved_session(sid, title)
def tag_saved_session(sid, tag): return _sdk.tag_saved_session(sid, tag)
def delete_saved_session(sid): return _sdk.delete_saved_session(sid)
def fork_current_session(title=None, up_to_message_id=None): return _sdk.fork_current_session(title, up_to_message_id)

# Advanced features (Phase 5)
def rewind_to_message(mid): return _sdk.rewind_to_message(mid)
def get_mcp_status():      return _sdk.get_mcp_status()
def reconnect_mcp(name):   return _sdk.reconnect_mcp(name)
def toggle_mcp(name, on):  return _sdk.toggle_mcp(name, on)
