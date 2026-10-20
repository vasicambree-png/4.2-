# Codex + DeepSeek + GitHub 接续工具

这套工具从任意已配置 GitHub 的本地 Git 仓库运行。DeepSeek 使用 Codex 执行，任务完成后按任务要求验证、提交和推送；官方 GPT 可以读取本机保存的 DeepSeek 对话继续分析。官方登录和已有 relay profile 保留。

## 可见模型列表入口

原生桌面配置见 `使用DeepSeek模型列表.md`。安装后重新打开 Codex，新建聊天即可在 DeepSeek 模式中选择 Flash / Pro 并粘贴网页方案。`切换Codex模型.cmd` 提供可视的 DeepSeek / 官方 GPT 模式切换，切换后需重新打开 Codex。官方模式原值已备份，登录文件保持原样。

以下终端工作流仍可使用。启动器和原生桌面模式是两个入口：终端启动器只覆盖本次参数；原生模式切换按需修改用户级的模型、供应商、目录和联网相关选项。

## 已配置的入口

- 模型：`deepseek-flash`，Responses 地址：`https://api.deepseek.com/`。
- 密钥：Windows 凭据管理器的 `CodexDeepSeekBridge/API`。终端启动器通过子进程环境变量传入，原生桌面通过 Codex 的认证命令读取。未写入代码、TOML、任务单或 Git 仓库。
- Codex：优先使用正常 `codex.cmd` 所属的原生可执行文件；本机核实为 CLI 0.156.1。
- 依赖：Windows、现有 Python、Git、Codex CLI。工具没有安装新依赖。
- 对话：沿用现有 `CODEX_HOME`。执行日志另存于本工具所在工作区的 `work/bridge-runs/`。每次输出会话 ID 和 `codex://threads/<ID>`。
- 终端启动器只使用本次启动参数，不直接修改用户级 config/profile。原生桌面切换入口会备份并修改必要的模型配置，点击恢复官方模式可还原原值；登录文件和 relay profile 保留。
- 正常任务继承用户已有的 `sandbox_mode` 和 `approval_policy`；本机现有配置为 danger-full-access / never。只读调用与隔离测试分别使用只读和 workspace-write。Git 提交需要 `.git` 写权限，GitHub 同步需要本机凭据与网络权限，限制这些权限的环境会明确报告未完成。

## 网页版 → DeepSeek → GitHub

1. 在真实项目中运行 `git remote -v`、`git branch --show-current`、`git rev-parse HEAD`，记录仓库、分支和完整提交 SHA。
2. 让网页版 GPT 读取该仓库和提交，并先确认读到的版本。GitHub 连接不保证瞬间刷新索引；无法确认版本时，提供提交链接或所需源码。
3. 网页版按 `task.example.json` 生成任务单，填写真实 baseline、文件范围、详细要求和验证命令。确认方案后保存成 JSON。模板中的示例文字不是可执行任务。
4. 在普通 PowerShell 中启动（路径请替换为本工具的实际路径）：

```powershell
& '<工具路径>\Start-Bridge.ps1' -Repo 'D:\实际项目' -Task 'D:\实际项目\docs\ai\TASK-001.json'
```

启动器在付费调用前核对 HEAD、origin URL 和分支；不匹配则停止。DeepSeek 按项目 AGENTS 和任务单执行，必要验证后只暂存本任务文件，正常 commit/push。任务中必须写出真实验收要求；模型完成回复不代表验证通过。

5. 把最终 commit 链接和执行结果交给网页版 GPT。要求它核实新提交、审查差异，再生成下一轮任务单。推荐把不含秘密的任务单和简短结果保存到各项目 `docs/ai/` 并随任务提交。

完整对话、密钥、执行日志保留本机。不要让操作员把这些文件上传 GitHub。构建、烧录和硬件观察分别报告；此工具不授权烧录或硬件动作。

## 额度恢复后打开 DeepSeek 对话

运行结束会输出 `codex://threads/<ID>`，可在 Codex 打开对应本机会话。本轮已验证桌面能读取 DeepSeek 保存的历史。它是 Codex 本地会话，不是 DeepSeek 官网账号的网页聊天。

推荐用已验证的纯文本接续方式：

```powershell
& '<工具路径>\Start-Bridge.ps1' -Repo 'D:\实际项目' -Provider gpt -Model gpt-6-sol -Handoff '<DeepSeek会话ID>' -Prompt '读取之前的执行记录，重新检查当前源码和Git状态，继续本任务。'
```

这会启动官方 GPT 的新会话，并带上前一个会话的用户、助手消息和工具记录；不会发送内部推理。原 DeepSeek 会话仍保留。若历史超过 200000 字符，改用经检查的任务单与执行报告。

本轮已实测 GPT 成功读取 DeepSeek 记录。直接跨供应商 `fork` 出现接口内容格式错误，因此 `-Fork` 属于高级选项，尚未验证可用于跨模型接续；同供应商恢复可使用 `-Resume`，仍需按真实运行结果判断。

本机原官方默认 `gpt-6.1-sol` 被终端测试接口拒绝；`gpt-6-sol` 来自当时账号模型列表，并已成功调用。恢复官方桌面模式时仍还原用户原值 `gpt-6.1-sol`。可用模型和额度以账号当时状态为准。

## 密钥维护和诊断

密钥曾发到聊天，建议在 DeepSeek 控制台轮换，然后用隐藏输入替换：

```powershell
python '<工具路径>\bridge.py' key-set
python '<工具路径>\bridge.py' doctor
```

`doctor` 只检查认证和模型列表，不做模型推理。删除本工具专用凭据：

```powershell
python '<工具路径>\bridge.py' key-remove
```

独立导出对话：

```powershell
python '<工具路径>\bridge.py' export --thread '<会话ID>' --output 'D:\你的记录目录\DeepSeek对话.md'
```

导出会过滤常见 `sk-` 格式密钥，但不是所有秘密的完整检测器；导出记录应保留本机。

## 验证边界

见 `VERIFICATION.md`。该工具不自动登录 ChatGPT 网页、不向网页版发送消息、不持续监控额度，也不自动用尽额度后切换模型。你选择启动哪个模型，GitHub 传递确定版本的任务和结果，符合本次优先选择的任务单闭环。

官方依据：[DeepSeek Codex 接入](https://api-docs.deepseek.com/quick_start/agent_integrations/codex/)、[Codex 配置](https://developers.openai.com/codex/config-advanced/)、[DeepSeek Responses 兼容性](https://api-docs.deepseek.com/guides/responses_api/)。
