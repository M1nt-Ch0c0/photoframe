# APP 自己收图：2026-10-23 Mac 验收

本记录对应功能分支 `codex/photoframe-ble-input`，协议见 `ble-image-input.md`。
宿主仍为 `b4709c0`，未刷写宿主、修改分区表、SDK、导入表或 Registry 组件。
`esp32s3`、各仓 README、AGENTS 与交接时未提交的文档草稿均保留，没有混入功能提交。

## 构建与代码

- 相框代码 `3b09a00`：真实 APP 入口、分块接收、APP 内 SHA-256、完成回执及生命周期清理。
- 电脑端基础实现 `3008eb0`：新主路径使用签名应用特征读写，保留旧宿主通道实现和回归。
- ESP-IDF 为固定 `5e6f53cdb31fe5708eae3f55af9737be2822db22`，子模块状态与该提交一致。
  elf_loader 为未修改的 Registry 1.3.3；SDK 头文件副本与宿主 byte-identical。
- doctor strict 通过工具链检查，加载现有 Python 3.12 IDF 环境和 Chrome 路径。
  没有打开串口。doctor 枚举出 USB 设备，说明当前 USB 连线状态可能不同于 HANDOFF 的独立供电快照；
  本轮只通过 BLE 观察 uptime/reset reason，不据此宣称证明了独立供电条件。
- 原生 3 组 CTest 通过；同套真实 C APP/解码器用 ASan/UBSan 构建并通过。
- 宿主 86 项 Python/C 测试全部通过，包含真实新相框 ELF 导入校验及实际 C 协议 ASan/UBSan。
  Mac 缺少 OpenSSL 测试依赖，因此从官方源下载 3.5.4、验证发布 SHA-256，
  在 `/tmp/photoframe-test-deps` 构建，仅给测试提供 include/lib 路径，没有修改系统依赖或宿主代码。
- 电脑端 Go 1.26.0 `make check`（Chrome、unit、race、vet、原生 CGO build）通过。
  Windows amd64 与 Linux amd64 静态交叉构建通过；未在这两个系统重新做无线测试。
- 旧 `.so` 与显式 `PHOTOFRAME_ABI=1` 的 app 均成功构建；旧 `.so` 没有新增 APP BLE 导入。

| 产物 | 大小 | SHA-256 |
|---|---:|---|
| ABI 2.1 `photoframe.app.elf` | 75300 | `0fd32e479dfe446ac4882117628f43b9de87bf36d52373f8b5060b7d143bc591` |
| `photoframe-v17.pkg` | 79396 | `3a2b3df17b2d898a22bc9f8b613671d31f4153231c5b94e61ce381b97f3672d6` |
| 兼容 `photoframe.so` | 75144 | `abde29f3bef1443a632ffcd2ea891957d66d057d13c4499f3ad2e22a6dc56ab2` |

新 ELF `.text=50795`、`.rodata=14354`、`.bss=504` 字节；这些是应用 ELF 的节大小，
不能当成宿主静态 DIRAM 或整机峰值内存。运行时内存证据如下。

## 发现与安装

通过原有 PhotoPainter Renderer 应用身份鉴权，初始签名状态：稳定 ID `9c139eb063e8`，
运行 photoframe v15、ABI 1，版本槽 `[15,16]`。新包 v17 成功写入备用槽 B，
状态仍运行 v15、pending=1、版本 `[15,17]`，原活动 v15 保留。

第一次安装在连接阶段超时，没有上传进度；查询证明仍无 pending，再重试暂存成功。
显式 activate 只发送一次；动态服务变化期间 CLI 返回 Read timeout，随后重新鉴权查询，
确认 v17、payload ABI 2、active B、previous A、ready/runtime_ready/confirmed=true，
服务 UUID `a4fe200015364c88a2ce32ecdaac0471`、两个特征、generation 6 / revision 19。
没有把 `open` 返回码当成激活证据，也没有重复激活。

## APP 收图安全检查

`TestHardwareAppImageSafety` 通过，97.89 秒。每次连接都要求设备 IPv4 为 `0.0.0.0`。

| 时点 | 内部空闲 RAM | 空闲 PSRAM | uptime ms |
|---|---:|---:|---:|
| APP 接收前 | 163447 | 8281640 | 3341209 |
| APP 预约 5 MiB 并收到部分数据 | 163447 | 3038756 | 3343549 |
| 断线重连、连接边界处理后 | 163615 | 8283216 | 3347599 |
| 显式取消后 | 163443 | 8281640 | 3349929 |
| 保持宿主连接、APP 空闲超时后 | 163443 | 8281640 | 3418099 |

5 MiB 预约使 PSRAM 减少 5242884 字节，内部空闲 RAM 不变；断线、取消和 APP 自己的
60 秒超时都释放接收内存。超时测试每 22 秒只发宿主 INFO，66 秒后 APP 回 canceled/error 9，
证明 APP 超时并非依靠宿主会话断开。坏 PNG 的 SHA 匹配后交给真实解码器，回 failed/error 7，
未触发有效图刷新；应用版本和 ready 保持。reset reason 一直为 1，uptime 持续增加。

首轮检查在断线释放后遇到签名宿主 busy，未完成后续项目。已修正客户端：仅对明确的
共享接纳 busy（宿主码 3，证明事件未接纳）延迟重试；传输丢失、鉴权错误和不确定 COMMIT
绝不以该逻辑重复。修正后完整测试通过，并增加模拟定时竞争的回归测试。

## 待核验的实机项目

有效图的 APP 完成回执与丢回复恢复、切出服务/旧代次失效与切回、应用回滚、真实 GPIO 4
切换及人工目测仍须以本轮最终记录为准。常驻服务在完成这些验收之前保持禁用。
最大 5 MiB 的证据目前仅为预约和释放，不代表完整传输 5 MiB 或长期耐久。

## APP 实体刷新与丢回复

`TestHardwareAppImageDisplay` 通过，247.71 秒。输入是已由用户确认主题的
`mint-skin` PNG，59124 字节。上传完成耗时 219.8 秒；故意丢弃 APP COMMIT 的签名回复，
重连只 QUERY 同一 ID，在 245.5 秒恢复匹配 ID/长度/SHA 的 complete/OK 回执。
严格只有一次 COMMIT、一次 QUERY。全程设备 IPv4 为 `0.0.0.0`，没有调用宿主图片 opcode。
该回执由 APP 在同步显示及最终 POWER_OFF 成功后生成。

显示前内部空闲 163447、PSRAM 8281640、uptime 3437599 ms；显示后分别为
163623、7898188、3679879 ms，reset reason 均为 1。PSRAM 的约 384 KB 差值是成功后可选帧缓存，
不是遗留接收缓冲。该测试窗口没有重启，但实体画面方向/颜色/主题仍需本轮人工目测。

### 测试完成后的 POWERON 重启

随后安装新电脑端运行文件并准备回滚时，连接发现超时。仅查询状态，没有盲目重发回滚；
签名状态表明仍为 v17、active B/previous A，但 generation 1、last_switch_source=boot、
uptime 63829 ms、reset reason 1，cache_bytes=0，证明在已通过的显示测试之后发生了 POWERON 重启。
这不推翻已取得的那次完成回执，也不能据此宣称长时间供电稳定。
doctor 此前枚举到了设备 USB 串口；已请用户核实独立 USB 供电和 Mac 数据线状态。
不打开串口、不改变宿主，等待供电状态核实后再做剩余实体操作和最终目测。

已安装的新主路径电脑端 SHA-256：
`aca660cbca11415d53208416cbd73eea54a6c298b4b689dc7b326099f65f119e`。
原电脑端已在仓库外保留回退副本，SHA-256 与 HANDOFF 的 `8c9251a9...` 一致。
运行配置和应用身份未修改，常驻服务继续禁用；没有读取日常服务日志或输出私有配置。
