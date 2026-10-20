# 在 Codex 中选择 DeepSeek

已增加原生桌面的 DeepSeek 模型目录，包括 `DeepSeek-Flash`（`deepseek-flash`）和 `DeepSeek-V4-Pro`（`deepseek-v4-pro`）。API 密钥继续从 Windows 凭据管理器读取。

1. 配置完成后，退出并重新打开 Codex。
2. 在真实项目下新建聊天，打开输入框附近的模型选择器，选择 `DeepSeek-Flash` 或 `DeepSeek-V4-Pro`。部分客户端会把当前供应商标为 `Custom`，以模型列表和实际调用结果为准。
3. 将网页版 GPT 的修改方案粘贴到该新聊天，要求它执行、验证，并按项目约定提交和推送 GitHub。
4. 将最终提交链接交回网页版 GPT 审查。

额度恢复后，在 `D:\蓝牙模块优化\tools\ai-bridge` 双击 `切换Codex模型.cmd`，点击“恢复官方 GPT 模式”，然后退出并重新打开 Codex。切回 DeepSeek 时，点击“DeepSeek 模式 · Flash / Pro”并重新打开即可。

本机 Codex 桌面 26.928.1915.0 一次使用一个供应商。模式切换不删除官方登录凭据或已有对话；官方配置的模型、供应商和联网选项会恢复为本次安装前记录的值。新增的 DeepSeek provider 仍保留，方便再次切换。

使用新聊天传递网页方案。原 GPT 聊天可能继续使用其保存的供应商；跨模型完整推理历史存在兼容问题，不能只在旧聊天里更改模型名称就认为已转到 DeepSeek。

如需继续前一模式的执行记录，可使用此前工具的 `--handoff` 入口；记录仍保存在本机 Codex。模型目录与供应商必须匹配，官方 GPT 和 DeepSeek 在当前版本不能混在一个列表中自动选择 API。

桌面显示需要重启后检查。本轮通过本机前端实现和后端 `model/list` 校验配置；没有自动点击官方 Codex 界面。

依据：[DeepSeek 官方 Codex 接入](https://api-docs.deepseek.com/quick_start/agent_integrations/codex/)、[OpenAI 自定义供应商配置](https://developers.openai.com/codex/config-advanced/)。
