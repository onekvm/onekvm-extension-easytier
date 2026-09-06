#!/usr/bin/env python3
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
start = text.find("fn default_machine_id_state_dir() -> anyhow::Result<PathBuf> {")
end = text.find("\nfn read_state_machine_id", start)
if start < 0 or end <= start:
    sys.exit(0)
if "cfg_select!" not in text[start:end]:
    sys.exit(0)
linux = (
    "fn default_machine_id_state_dir() -> anyhow::Result<PathBuf> {\n"
    "    Ok(default_linux_machine_id_state_dir(\n"
    '        env::var_os("XDG_DATA_HOME"),\n'
    '        env::var_os("HOME"),\n'
    "    ))\n"
    "}\n"
    "\n"
)
path.write_text(text[:start] + linux + text[end + 1 :])
