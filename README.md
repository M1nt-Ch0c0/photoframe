# photoframe

**文档发布说明：本次 main 仅更新文档。** ABI 2.1 SDK/echo 的公开代码基线为 `d2dd482`，见 [`codex/ble-discovery-app-ota`](https://github.com/M1nt-Ch0c0/photoframe/tree/codex/ble-discovery-app-ota)。Mac 已在本地 `codex/photoframe-ble-input` 实现相框收图并安装 v17；该功能分支尚未推送，不能从 main 获得该实现。

> **AI / 开发者入口：**先阅读 [`AGENTS.md`](AGENTS.md)。空白电脑部署、三仓联调和真机诊断使用 [`develop-photopainter-stack`](https://github.com/M1nt-Ch0c0/photopainter-host/blob/main/.agents/skills/develop-photopainter-stack/SKILL.md)。

四个仓库的职责、架构图、耦合边界与修改影响，见 [PhotoPainter 架构总览](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/ARCHITECTURE.md)。

**应用开发从 [开发者 Wiki](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/wiki/Home.md) 开始。** 本仓库维护应用入口、业务输入、图片验证和示例；宿主框架只提供运行、鉴权、更新和通用服务。

2026-10-23 已批准把 BLE 收图状态机、校验、断线处理与完成回执迁入 photoframe ABI 2.1 APP。本文对应的 `d2dd482` 功能基线尚未实现该路线；普通相框仍声明 2.0，BLE echo 是已实现的 2.1 示例。Mac 本地功能分支已实现并测过有效图与回执，尚未推送及完成最终验收；后续开发和验收见 [APP 收图路线](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/wiki/BLE-Image-App-Roadmap.md)，不要把旧宿主 PNG 通道当作已经交付的新 APP 服务。

默认 `.app.elf` 已使用 ABI 2：启动时恢复缓存或显示 `WAITING FOR IMAGE`，收到 PNG 后更新屏幕；宿主负责六色逻辑帧的板级显示。必须先部署支持 ABI 2 的新宿主。完整规则见 [运行层指南](https://github.com/M1nt-Ch0c0/photopainter-host/blob/main/docs-runtime-v2.md)。

## 构建与安装

SDK 现在包含兼容的 ABI 2.1 BLE 扩展；普通相框和旧应用仍声明 2.0。需要应用自定义蓝牙服务时使用 `APP_MANIFEST_BLE`，参考 [BLE 回显示例](examples/ble-echo/README.md)。宿主保存 GATT 描述和数据副本，通过事件交付写入，应用切出时清理服务；应用不直接控制蓝牙协议栈、发现广播或安装通道。

本仓库为 7.3 英寸、800×480、六色 Spectra 6 PhotoPainter 提供独立可加载组件，不是整机固件。使用 ESP-IDF commit `5e6f53cdb31fe5708eae3f55af9737be2822db22`（约 v6.0.3），分别构建应用 ELF 与共享对象：

```sh
. /path/to/esp-idf/export.sh

idf.py -B build-app -DIDF_TARGET=esp32s3 \
  -DPHOTOFRAME_ARTIFACT=app build
idf.py -B build-so -DIDF_TARGET=esp32s3 \
  -DPHOTOFRAME_ARTIFACT=so build
```

产物为 `build-app/photoframe.app.elf` 与 `build-so/photoframe.so`。`main/idf_component.yml` 从 Espressif Component Registry 解析 `espressif/elf_loader: ^1.3.3` 与 `espressif/libpng: ^1.6.58~1`；`dependencies.lock` 固定本次解析结果，其中 elf_loader 为 1.3.3。依赖不 fork、不 vendor。

业务 app ELF 通过宿主的独立 A/B 槽更新，ABI 兼容时无需重建或刷写主程序。不要刷写组件构建目录内的普通 `.bin`。打包、上传、试运行及回退步骤见 [宿主双槽说明](https://github.com/M1nt-Ch0c0/photopainter-host/blob/main/docs-module-slots.md)。

本机解码与打包测试：

```sh
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
python3 -m unittest discover -s examples/color-test -p 'test_*.py'
```

显示引脚和 E6 初始化、刷新、关断时序取自 Waveshare [`ESP32-S3-PhotoPainter` commit `a5e8f757`](https://github.com/waveshareteam/ESP32-S3-PhotoPainter/tree/a5e8f757ba0cafbb5586f07d3e83bda3184c0845/01_Example/xiaozhi-esp32)。SCLK/MOSI/DC/CS/RST/BUSY 固定为 GPIO 10/11/8/9/12/13；AXP2101 通过 GPIO 48/47 上的 I2C0 验证芯片 ID，配置并回读 ALDO4 3.3 V 后才允许访问 EPD GPIO/SPI。上游 MIT notice 保存在 `LICENSES/Waveshare-PhotoPainter-MIT.txt`，本仓库原创代码使用 `LICENSE` 中的 MIT 许可。

## 推图

现有兼容实现由宿主通过 HTTP 或专用 BLE 通道接收 PNG，再由同一 App Manager 交给当前 APP；其旧客户端为 `ai-quota-frame ble push --file frame.png`。它支持旧 ABI 1/2 输入，但不是新的 APP BLE 收图协议。新的主路径将由本 APP 的 2.1 服务接收，电脑客户端也需相应适配；当前未发布该业务服务的 UUID/命令契约。两种路线都不能抢占当前应用，发现、鉴权和 OTA 始终由宿主提供。兼容命令见[电脑端指南](https://github.com/M1nt-Ch0c0/ai-quota-frame/blob/main/docs/ble.md)。

共享对象公开 ABI：

```c
int photoframe_render_png(const uint8_t *png_data, size_t png_size);
```

宿主必须串行调用；输入在返回前保持有效。PNG 不得超过 5 MiB，必须恰好 800×480、非隔行，并且每个像素只能是完全不透明的黑 `#000000`、白 `#ffffff`、黄 `#ffff00`、红 `#ff0000`、蓝 `#0000ff`、绿 `#00ff00`。组件会完整解码、验色并拒绝尾随数据，然后执行 180° 像素打包；验证失败不会接触面板。成功仅在最终 POWER_OFF 的 BUSY 等待完成后返回。

仅旧 ABI 1（显式 `-DPHOTOFRAME_ABI=1` 构建）由宿主解析下列三个符号。默认 ABI 2 使用 `components/app_sdk/include/photopainter_app.h` 的事件与宿主服务，PNG 解码后生成 384000 字节逻辑帧，由宿主执行唯一的旋转和硬件刷新：

```c
const uint8_t *photoframe_host_png_data(void);
size_t photoframe_host_png_size(void);
void photoframe_host_report_result(int result);
```

必须实现结果回调，因为 elf_loader 1.3.3 的 `esp_elf_request()` 不传播入口返回值。稳定结果码如下：

| 值 | 名称 | 含义 |
|---:|---|---|
| `0` | `PHOTOFRAME_OK` | 刷新及最终关断等待完成 |
| `-1` | `PHOTOFRAME_ERR_ARGUMENT` | 空或超限输入 |
| `-2` | `PHOTOFRAME_ERR_PNG` | PNG 解码或结构错误 |
| `-3` | `PHOTOFRAME_ERR_GEOMETRY` | 尺寸错误或隔行 PNG |
| `-4` | `PHOTOFRAME_ERR_COLOR` | 存在六色以外或非不透明像素 |
| `-5` | `PHOTOFRAME_ERR_ALLOCATION` | 面板 I/O 前的组件内存分配失败 |
| `-6` | `PHOTOFRAME_ERR_IO` | AXP2101、I2C、GPIO、SPI、初始化或清理失败 |
| `-7` | `PHOTOFRAME_ERR_BUSY_TIMEOUT` | 低有效 BUSY 未在 120 秒内恢复高电平 |

## 鉴权

组件不保存密钥，也不实现网络鉴权。HTTP Bearer 和 BLE HMAC 只由宿主固件校验；宿主检查鉴权和请求大小后，才把内存缓冲区交给本组件，完整 PNG 验证由组件在触碰面板前完成。HTTP 200 或 BLE 鉴权完成凭据只在实体刷新和最终 POWER_OFF 等待结束后返回，仍需人工确认显示效果。不得把 Wi-Fi 密码、推图码、NVS 镜像或其他密钥提交到此仓库。

电源映射依据：[官方原理图](https://files.waveshare.com/wiki/ESP32-S3-PhotoPainter/ESP32-S3-PhotoPainter-Schematic.pdf)中 AXP2101 的 pin 15 / ALDO4 连接 `EPD_VCC`，pin 16 / ALDO3 连接 `Audio_VCC`。屏幕电压寄存器为 `0x95`，使能为 `0x90` 的 bit 3；读改写保留其他电源位。

## 独立第二应用示例

[color-test](examples/color-test/README.md) 是独立构建和安装的六色色条应用，可用于验证多个应用各自的 A/B 更新、切换与回退。它在 START 自主绘制色带，使用宿主显示服务，不接受 PNG，也不修改原 photoframe 入口。另有 [无显示生命周期示例](examples/lifecycle/README.md)。
