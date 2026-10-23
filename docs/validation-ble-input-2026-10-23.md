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

应用双向回滚已在当前 USB 连接下通过。切出服务/旧代次失效与切回有部分通过证据，
但生命周期整项因连接失败及一次重启尚未通过，详见最后一节；真实 GPIO 4 和人工目测仍未验证。
有效图的 APP 完成回执与丢回复恢复已通过，见下一节。常驻服务在完成验收之前保持禁用。
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
当时暂缓了剩余实体操作。独立供电是排查对照条件，并非 BLE 的技术要求；
不能仅凭 USB 枚举和 POWERON 复位码认定 Mac USB 是重启原因。

已安装的新主路径电脑端 SHA-256：
`aca660cbca11415d53208416cbd73eea54a6c298b4b689dc7b326099f65f119e`。
原电脑端已在仓库外保留回退副本，SHA-256 与 HANDOFF 的 `8c9251a9...` 一致。
运行配置和应用身份未修改，常驻服务继续禁用；没有读取日常服务日志或输出私有配置。

## 当前 USB 连接下继续验收

用户明确要求「再试一次」，继续当前 USB 供电/数据连接条件下的 BLE 操作。
仍不开串口、不刷宿主；按每次签名状态核对版本、运行代次和启动时间。

重试前签名状态与上一次不同：运行 ABI 1 v15，active A、previous/pending 均为 -1，
槽版本 `[15,17]`，generation 1、last_switch_source=boot、uptime 483489 ms、reset reason 1。
这符合 `app_boot_fallback()` / `module_slots_recover()` 将 previous 恢复为 active 并清除回退标记的状态；
没有串口记录，不能确定是哪次 START 失败或中断触发。设备鉴权发现仍为 IPv4 `0.0.0.0`。

保留活动 v15，将同一 ELF（SHA 未变）重新打包为 v18，79396 字节，包 SHA-256
`16a569c6e85efeee2aaf8d4b6a8c673c62e02fbc6f6753f7e588734b1df521ef`。
BLE 上传得到签名暂存成功响应；随后单次显式激活成功，v18、active B/previous A、
ready/runtime_ready/confirmed=true，generation 2、service revision 5，uptime 653739 ms。

显式回滚 v18→v15 成功：active A/previous B、generation 3、uptime 817209 ms。
ABI 1 v15 的 ready=true 表示已加载；runtime_ready/confirmed=false 符合尚未 INPUT 的状态，
不能把它误判为 ABI 2 START 失败。再执行一次 rollback 回 v18；该次命令遇到 GATT 读取超时，
没有重发，重新鉴权查询确认 active B/previous A、ready/runtime_ready/confirmed=true，
generation 4、service revision 11、uptime 876179 ms。槽版本保持 `[15,18]`，
reset reason 一直为 1，以上安装、激活和双向回滚期间 uptime 连续，无重启证据。

随后将现有 ABI 2 色条 ELF 打包为 `color-test v4`（5732 字节），暂存备用 B 槽并显式激活。
签名状态确认 v4、ABI 2、active B/previous A、ready/runtime_ready/confirmed=true，
generation 5、uptime 936729 ms，色条与相框均 button_eligible=true；旧色条 v1 保留。
单次 BLE SWITCH 返回相框时读取超时，首次状态查询也超时；未重复 SWITCH，
后续签名状态确认 v18 正常运行、generation 6、revision 17、uptime 1025952 ms。
reset reason 仍为 1。这说明本轮超时并非重启证据，服务变化后的重连仍有短暂不稳定。

### 生命周期重试：部分证据通过，整项尚未通过

首次 `TestHardwareAppImageLifecycle` 在 54.35 秒失败于恢复后的鉴权挑战读取。
在此之前，5 MiB 接收期间切出成功，PSRAM 从接收前 8281640 恢复到色条运行时
8349372，旧 photoframe 服务不可用、旧代次 READ/WRITE 均回宿主错误 8。
返回相框的 SWITCH 回复丢失后没有重发；独立查询确认 v18 已恢复，generation 8、
revision 23、uptime 1127249 ms，无重启，PSRAM 8283216。

电脑端测试提交 `93bbe07` 仅给生命周期测试增加最多 3 次连接建立尝试；一旦进入
已鉴权回调，立即返回其结果，绝不重放其中的应用操作。硬件测试二进制重编通过，
`go test -tags photopainter_hardware ./internal/ble` 通过（硬件项默认不启用）。
正常客户端程序未改变。

再次完整运行在 27.51 秒失败，这次是真实重启：切出前 uptime 1183419 ms，
SWITCH 回复丢失后观察到 uptime 14147 ms、reset reason 1，未运行预期色条应用。
未重发切换。独立签名查询确认启动后仍为 photoframe v18、generation 1、source=boot、
ready/runtime_ready/confirmed=true、uptime 50956 ms，色条 v4 及两应用旧槽仍保留。
因此不能把生命周期整项标成 PASS，也不能仅凭 POWERON 复位码推断 USB 是原因。
已询问用户是否有拔插/复位操作；继续当前连接进行受控的正确主题恢复，不要求更换供电。

### v18 正确主题恢复与完成回执

恢复主题的首次连接在 DiscoverServices 阶段超时，尚未进入已鉴权测试回调、上传或 COMMIT。
再次独立连接后，`TestHardwareAppImageDisplay` PASS，250.63 秒。
仍用相同的 59124 字节 `mint-skin` PNG：222.5 秒传完，故意丢弃 COMMIT 回复后，
247.5 秒取得原请求的匹配完成回执，严格一次 COMMIT、一次 QUERY，证明物理刷新及 POWER_OFF 完成。
本次显示前/后 uptime 为 62649/304879 ms、reset reason 1，窗口内未重启；
内部空闲 RAM 163579/163571，PSRAM 8281656/7896612，差值包含成功后的帧缓存。
此窗口成功不消除前述切换时重启，不能据此宣布完整生命周期或长期稳定性验收通过。

测试后已原子恢复正常客户端程序（SHA `aca660cb...`），没有留下硬件测试二进制作为后台程序。
常驻服务保持未加载，客户端进程为 0；配置及用户已有文档草稿未改。
本次新路径画面的方向/颜色/内容已请求人工确认，尚未收到回复。
最终独立签名查询仍为 v18、active B/previous A、ready/runtime_ready/confirmed=true，
uptime 362719 ms、cache_bytes 384000；较完成后的 304879 ms 连续增长，未见新增重启。
