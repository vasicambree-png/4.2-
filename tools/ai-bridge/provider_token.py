"""Internal Codex credential command. Its stdout goes directly to Codex."""
import sys
from credentials import read_key

if __name__ == '__main__':
    if sys.argv[1:] != ['token']:
        raise SystemExit('Internal credential helper; called by Codex only.')
    try:
        sys.stdout.write(read_key())
    except Exception:
        print('DeepSeek credential unavailable; use bridge.py key-set.', file=sys.stderr)
        raise SystemExit(1)
