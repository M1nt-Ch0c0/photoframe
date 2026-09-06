# photoframe

> **AI / 开发者入口：**先阅读 [`AGENTS.md`](AGENTS.md)。空白电脑部署、三仓联调和真机诊断使用 [`develop-photopainter-stack`](https://github.com/M1nt-Ch0c0/photopainter-host/blob/main/.agents/skills/develop-photopainter-stack/SKILL.md)。

四个仓库的职责、架构图、耦合边界与修改影响，见 [PhotoPainter 架构总览](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/ARCHITECTURE.md)。

## 刷写

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
```

显示引脚和 E6 初始化、刷新、关断时序取自 Waveshare [`ESP32-S3-PhotoPainter` commit `a5e8f757`](https://github.com/waveshareteam/ESP32-S3-PhotoPainter/tree/a5e8f757ba0cafbb5586f07d3e83bda3184c0845/01_Example/xiaozhi-esp32)。SCLK/MOSI/DC/CS/RST/BUSY 固定为 GPIO 10/11/8/9/12/13；AXP2101 通过 GPIO 48/47 上的 I2C0 验证芯片 ID，配置并回读 ALDO4 3.3 V 后才允许访问 EPD GPIO/SPI。上游 MIT notice 保存在 `LICENSES/Waveshare-PhotoPainter-MIT.txt`，本仓库原创代码使用 `LICENSE` 中的 MIT 许可。

## 推图

共享对象公开 ABI：

```c
int photoframe_render_png(const uint8_t *png_data, size_t png_size);
```

宿主必须串行调用；输入在返回前保持有效。PNG 不得超过 5 MiB，必须恰好 800×480、非隔行，并且每个像素只能是完全不透明的黑 `#000000`、白 `#ffffff`、黄 `#ffff00`、红 `#ff0000`、蓝 `#0000ff`、绿 `#00ff00`。组件会完整解码、验色并拒绝尾随数据，然后执行 180° 像素打包；验证失败不会接触面板。成功仅在最终 POWER_OFF 的 BUSY 等待完成后返回。

应用 ELF 另由宿主解析三个符号：

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

组件不保存密钥，也不实现网络鉴权。Bearer 推图码只由宿主固件校验；宿主验证鉴权、请求大小和 PNG 签名字节后，才把内存缓冲区交给本组件，完整 PNG 验证由组件在触碰面板前完成。不得把 Wi-Fi 密码、推图码、NVS 镜像或其他密钥提交到此仓库。

电源映射依据：[官方原理图](https://files.waveshare.com/wiki/ESP32-S3-PhotoPainter/ESP32-S3-PhotoPainter-Schematic.pdf)中 AXP2101 的 pin 15 / ALDO4 连接 `EPD_VCC`，pin 16 / ALDO3 连接 `Audio_VCC`。屏幕电压寄存器为 `0x95`，使能为 `0x90` 的 bit 3；读改写保留其他电源位。
