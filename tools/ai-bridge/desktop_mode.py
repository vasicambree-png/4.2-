"""Switch the native desktop model catalog; preserve official login and other config."""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import sys
import tomllib

ROOT = Path(__file__).resolve().parent
PROVIDER = 'deepseek_desktop'
KEYS = ('model', 'model_provider', 'model_catalog_json', 'web_search',
        'model_reasoning_effort', 'model_reasoning_summary')

def paths():
    directory = Path(os.environ.get('CODEX_HOME', str(Path.home() / '.codex')))
    return directory, directory / 'config.toml', directory / 'deepseek-desktop-switch.json'

def update_top_level(text, values):
    newline = '\r\n' if '\r\n' in text else '\n'
    lines = text.splitlines(keepends=True)
    split = next((i for i, line in enumerate(lines) if re.match(r'^\s*\[', line)), len(lines))
    top, tail = lines[:split], lines[split:]
    found = set()
    result = []
    for line in top:
        match = re.match(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=', line)
        key = match.group(1) if match else None
        if key in values:
            found.add(key)
            if values[key] is not None:
                result.append(key + ' = ' + json.dumps(values[key], ensure_ascii=False) + newline)
        else:
            result.append(line)
    for key, value in values.items():
        if key not in found and value is not None:
            if result and not result[-1].endswith(('\n', '\r')):
                result[-1] += newline
            result.append(key + ' = ' + json.dumps(value, ensure_ascii=False) + newline)
    return ''.join(result + tail)

def activate(mode):
    directory, config, state = paths()
    original = config.read_bytes()
    text = original.decode('utf-8')
    parsed = tomllib.loads(text)
    if not state.exists():
        if parsed.get('model_provider', 'openai') != 'openai':
            raise RuntimeError('Initial mode is not official OpenAI; inspect config before saving defaults.')
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
        backup = directory / ('backup-before-deepseek-visible-' + stamp)
        backup.mkdir(exist_ok=False)
        (backup / 'config.toml').write_bytes(original)
        snapshot = {'version': 1, 'official': {k: parsed.get(k) for k in KEYS},
                    'backup': str(backup), 'package_directory': str(ROOT)}
        state.write_text(json.dumps(snapshot, ensure_ascii=False, indent=2), encoding='utf-8')
    snapshot = json.loads(state.read_text(encoding='utf-8'))
    if mode == 'gpt':
        text = update_top_level(text, snapshot['official'])
    else:
        from credentials import read_key
        read_key()  # Confirm availability without printing or copying the key.
        package = Path(snapshot.get('package_directory', str(ROOT)))
        catalog = package / 'deepseek-desktop-models.json'
        models = json.loads(catalog.read_text(encoding='utf-8'))['models']
        if {m['slug'] for m in models} != {'deepseek-flash', 'deepseek-v4-pro'}:
            raise RuntimeError('DeepSeek model catalog is incomplete.')
        existing = parsed.get('model_providers', {}).get(PROVIDER)
        expected_python = str(Path(sys.executable).with_name('python.exe'))
        expected_helper = str(package / 'provider_token.py')
        if existing:
            if (existing.get('base_url') != 'https://api.deepseek.com/' or
                existing.get('auth', {}).get('args') != [expected_helper, 'token']):
                raise RuntimeError('Existing DeepSeek desktop provider differs; preserve it and inspect.')
        else:
            q = lambda value: json.dumps(value, ensure_ascii=False)
            text += '\n[model_providers.' + PROVIDER + ']\n'
            text += 'name = "DeepSeek"\nbase_url = "https://api.deepseek.com/"\nwire_api = "responses"\n'
            text += 'supports_websockets = false\n'
            text += 'auth = { command = ' + q(expected_python) + ', args = [' + q(expected_helper) + ', "token"], timeout_ms = 5000, refresh_interval_ms = 0 }\n'
        text = update_top_level(text, {
            'model': 'deepseek-flash', 'model_provider': PROVIDER,
            'model_catalog_json': str(catalog), 'web_search': 'disabled',
            'model_reasoning_effort': 'high', 'model_reasoning_summary': 'none'})
    checked = tomllib.loads(text)
    expected = PROVIDER if mode == 'ds' else snapshot['official'].get('model_provider', 'openai')
    if checked.get('model_provider', 'openai') != expected:
        raise RuntimeError('Config validation failed.')
    temporary = config.with_name('config.deepseek-switch.tmp')
    temporary.write_text(text, encoding='utf-8', newline='')
    os.replace(temporary, config)
    return checked.get('model_provider', 'openai'), checked.get('model')

def status():
    _, config, _ = paths()
    data = tomllib.loads(config.read_text(encoding='utf-8'))
    return data.get('model_provider', 'openai'), data.get('model')

def menu():
    import tkinter as tk
    from tkinter import messagebox
    window = tk.Tk()
    window.title('Codex 模型模式')
    window.geometry('460x340')
    window.resizable(False, False)
    window.configure(bg='#f3f6fa')
    label = tk.StringVar()
    def refresh():
        provider, model = status()
        label.set('当前配置：' + ('DeepSeek' if provider == PROVIDER else '官方 GPT') + '\n' + str(model))
    def choose(mode):
        try:
            activate(mode); refresh()
            messagebox.showinfo('配置完成', '请退出并重新打开 Codex。\n新建聊天后，在模型列表中选择模型并粘贴方案。', parent=window)
        except Exception as error:
            messagebox.showerror('配置未完成', str(error), parent=window)
    tk.Label(window, text='选择 Codex 模型模式', font=('Microsoft YaHei UI', 17, 'bold'), bg='#f3f6fa').pack(pady=(20, 8))
    tk.Label(window, textvariable=label, font=('Microsoft YaHei UI', 11), bg='#f3f6fa').pack(pady=6)
    tk.Button(window, text='DeepSeek 模式 · Flash / Pro', font=('Microsoft YaHei UI', 12), width=34, bg='#1f60d7', fg='white', command=lambda: choose('ds')).pack(pady=8)
    tk.Button(window, text='恢复官方 GPT 模式', font=('Microsoft YaHei UI', 12), width=34, command=lambda: choose('gpt')).pack(pady=8)
    tk.Label(window, text='切换模式后需要重新打开 Codex\n官方登录凭据及已有对话保留', font=('Microsoft YaHei UI', 10), bg='#f3f6fa', fg='#536175').pack(pady=10)
    refresh(); window.mainloop()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['ds', 'gpt', 'menu', 'status'])
    args = parser.parse_args()
    try:
        if args.mode == 'menu':
            menu()
        elif args.mode == 'status':
            print(*status())
        else:
            print('Configured:', *activate(args.mode))
            print('Reopen Codex and create a new conversation.')
    except Exception as error:
        print('ERROR:', error, file=sys.stderr)
        sys.exit(1)
