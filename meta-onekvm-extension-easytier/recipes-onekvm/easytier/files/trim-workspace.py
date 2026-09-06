#!/usr/bin/env python3
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
old = """members = [
    \"easytier\",
    \"easytier-gui/src-tauri\",
    \"easytier-rpc-build\",
    \"easytier-web\",
    \"easytier-contrib/easytier-ffi\",
    \"easytier-contrib/easytier-uptime\",
    \"easytier-contrib/easytier-android-jni\",
]
default-members = [\"easytier\", \"easytier-web\"]"""
new = """members = [
    \"easytier\",
    \"easytier-rpc-build\",
]
default-members = [\"easytier\"]"""
if old in text:
    path.write_text(text.replace(old, new, 1))
elif "easytier-gui" in text:
    raise SystemExit("EasyTier workspace members block not found")
