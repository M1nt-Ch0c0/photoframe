# photoframe

photoframe 是 PhotoPainter 相框上默认运行的**相框应用**：它通过蓝牙接收一张 800×480 的六色 PNG，校验后交给宿主固件刷到电子纸上，并回传完成回执。本仓库还包含三个示例应用和应用 SDK 头文件的副本。

这一页写给要构建、测试或修改 photoframe 的开发者。项目整体介绍、应用的运行原理和完整的写应用教程都在文档主站：

- [项目介绍](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/01-overview.md)：五个仓库各做什么
- [设备是怎么工作的](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/03-how-the-device-works.md)：宿主与应用、生命周期、A/B 槽、启动保护
- [编写一个应用](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/04-writing-an-app.md)：从建工程到装到设备上的完整步骤
- [应用接口参考](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/05-app-api-reference.md)：`photopainter_app.h` 里的所有接口

## 仓库内容

| 路径 | 内容 |
|---|---|
| `main/` | photoframe 应用入口和图片事务协议的实现 |
| `components/photoframe/` | PNG 解码与六色校验；旧 ABI 1 使用的电子纸驱动 |
| `components/app_sdk/` | 应用 SDK 头文件 `photopainter_app.h` 和构建辅助脚本，是 `photopainter-host/sdk/` 的副本 |
| `examples/` | 示例应用：[lifecycle](examples/lifecycle/README.md)、[color-test](examples/color-test/README.md)、[ble-echo](examples/ble-echo/README.md) |
| `tests/` | 在电脑上运行的原生测试 |
| `docs/ble-image-input.md` | [图片事务协议](docs/ble-image-input.md)的完整规格 |

## photoframe 做什么

photoframe 用 `APP_MANIFEST_CHANNEL` 声明 ABI 2.3，需要支持 ABI 2.3 的宿主固件。

- **启动时**：从宿主缓存恢复上一次显示的画面；没有缓存时显示 `WAITING FOR IMAGE` 等待页。
- **收图**：同时注册三种蓝牙通道（ABI 2.1 GATT 服务、ABI 2.2 CoC、ABI 2.3 通用消息），共用一套图片事务：BEGIN、CHUNK、COMMIT、CANCEL、QUERY。协议规格见 [图片事务协议](docs/ble-image-input.md)。
- **显示**：COMMIT 时校验 SHA-256，完整解码 PNG 并检查颜色，生成 384000 字节的六色逻辑帧，调用宿主的显示接口。旋转、像素打包和电子纸时序都由宿主完成。
- **完成回执**：显示接口在屏幕刷新完成并断电后才返回，photoframe 随后生成完成回执，同时把这一帧写进缓存。

PNG 必须满足：

- 最大 5 MiB；
- 恰好 800×480，非隔行；
- 每个像素完全不透明，只用黑 `#000000`、白 `#ffffff`、黄 `#ffff00`、红 `#ff0000`、蓝 `#0000ff`、绿 `#00ff00` 六种颜色；
- PNG 结束后没有多余数据。

photoframe 只负责收图和显示。蓝牙协议栈、配对与鉴权、应用安装和更新都由宿主固件负责。电脑端怎样推图见 [电脑端、控制台与网页插件](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/07-clients.md)，各组件的当前版本见 [版本与已知限制](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/09-versions-and-limits.md)。

## 构建

ESP 产物必须用固定的 ESP-IDF 提交 `5e6f53cdb31fe5708eae3f55af9737be2822db22`（v6.0.3）构建。`CMakeLists.txt` 会检查 `$IDF_PATH` 的提交，不一致直接报错。环境准备见 [编写一个应用](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/04-writing-an-app.md#准备环境)。

```sh
. /path/to/esp-idf/export.sh
idf.py -B build-app -DIDF_TARGET=esp32s3 -DPHOTOFRAME_ARTIFACT=app build
```

产物是 `build-app/photoframe.app.elf`。构建目录里的普通 `.bin` 是构建副产品，不能刷进设备。

依赖从 Espressif Component Registry 获取，`main/idf_component.yml` 声明版本范围，`dependencies.lock` 锁定为 `espressif/elf_loader` 1.3.3、`espressif/libpng` 1.6.58~1。

构建选项：

| 选项 | 取值 | 说明 |
|---|---|---|
| `PHOTOFRAME_ARTIFACT` | `app`（默认）或 `so` | 构建应用 ELF，或旧的共享对象 `photoframe.so` |
| `PHOTOFRAME_ABI` | `2`（默认）或 `1` | `1` 构建旧 ABI 1 应用；`so` 产物固定为 ABI 1 |

## 安装到设备

用宿主仓库的打包工具把 ELF 打成 `.pkg`，再用 `ai-quota-frame` 命令行或网页插件的开发者工具上传并激活。下面假设 `photoframe` 和 `photopainter-host` 是兄弟目录：

```sh
cd photopainter-host
python3 tools/module.py package --version 21 \
  --input ../photoframe/build-app/photoframe.app.elf \
  --output /tmp/photoframe-21.pkg

ai-quota-frame ble install  --file /tmp/photoframe-21.pkg
ai-quota-frame ble activate --app photoframe
```

`--version` 是发布号，用一个和设备上已有版本区分得开的数字。`install` 把包写进 photoframe 的备用槽，`activate` 试运行新版本，首屏成功后确认。完整说明，包括网页插件开发者工具的用法，见 [编写一个应用](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/04-writing-an-app.md#第五步装到设备上)。

## 测试

原生测试在电脑上运行，不需要设备和 ESP-IDF，需要 CMake、C 编译器、libpng 和 Python 3：

```sh
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
python3 -m unittest discover -s examples/color-test -p 'test_*.py'
```

| 测试 | 内容 |
|---|---|
| `tests/test_photoframe.c` | PNG 解码和校验，以及旧驱动的 180° 像素打包 |
| `tests/test_ble_image.c` | 用真实的应用入口和解码器、模拟的宿主服务，测试图片事务在三种通道上的行为 |
| `tests/test_app_entry.py` | 启动时的缓存恢复和等待页、INPUT 事件、缓存写入失败的处理 |
| `examples/ble-echo/test_entry.c` | ble-echo 示例的入口 |
| `examples/color-test/test_entry.py` | color-test 示例两种色条顺序的画面 |

测试里的显示接口都是替身。改动收图或显示逻辑后，还需要在设备上推一张图，确认拿到完成回执，并人工检查屏幕的内容、方向和颜色。

## 旧接口：ABI 1 和共享对象

默认的 ABI 2 应用不直接操作硬件。ABI 1 应用和 `photoframe.so` 是早期接口，为兼容保留，它们自带电子纸驱动，由 `components/photoframe/` 实现。

共享对象只导出一个函数：

```c
int photoframe_render_png(const uint8_t *png_data, size_t png_size);
```

调用方必须串行调用，输入在返回前保持有效。函数先完整解码和校验 PNG，校验通过后才访问电源芯片和屏幕，旋转 180° 后刷新，等最终 POWER_OFF 的 BUSY 信号结束才返回。

ABI 1 应用由宿主提供下面三个符号。`photoframe_host_report_result()` 用来回报结果，因为 elf_loader 1.3.3 的 `esp_elf_request()` 不传递入口函数的返回值：

```c
const uint8_t *photoframe_host_png_data(void);
size_t photoframe_host_png_size(void);
void photoframe_host_report_result(int result);
```

结果码：

| 值 | 名称 | 含义 |
|---:|---|---|
| `0` | `PHOTOFRAME_OK` | 刷新完成，屏幕已断电 |
| `-1` | `PHOTOFRAME_ERR_ARGUMENT` | 输入为空或超过 5 MiB |
| `-2` | `PHOTOFRAME_ERR_PNG` | PNG 解码失败或结构错误 |
| `-3` | `PHOTOFRAME_ERR_GEOMETRY` | 尺寸不是 800×480，或是隔行 PNG |
| `-4` | `PHOTOFRAME_ERR_COLOR` | 有六色以外的像素，或像素不是完全不透明 |
| `-5` | `PHOTOFRAME_ERR_ALLOCATION` | 访问屏幕前内存分配失败 |
| `-6` | `PHOTOFRAME_ERR_IO` | 电源芯片、I2C、GPIO 或 SPI 的初始化或清理失败 |
| `-7` | `PHOTOFRAME_ERR_BUSY_TIMEOUT` | BUSY 信号 120 秒内没有恢复 |

### 硬件细节

- 屏幕引脚：SCLK 10、MOSI 11、DC 8、CS 9、RST 12、BUSY 13。
- 电源芯片 AXP2101 通过 I2C0（SCL 48、SDA 47）访问。驱动先核对芯片 ID，再把 ALDO4 设为 3.3 V 并回读确认，之后才访问屏幕。
- 依据 [官方原理图](https://files.waveshare.com/wiki/ESP32-S3-PhotoPainter/ESP32-S3-PhotoPainter-Schematic.pdf)：ALDO4（pin 15）接 `EPD_VCC`，ALDO3（pin 16）接 `Audio_VCC`。ALDO4 电压寄存器为 `0x95`，使能位是 `0x90` 的 bit 3；修改时读改写，保留其他电源位。
- 屏幕的初始化、刷新和关断时序取自 Waveshare [`ESP32-S3-PhotoPainter`](https://github.com/waveshareteam/ESP32-S3-PhotoPainter/tree/a5e8f757ba0cafbb5586f07d3e83bda3184c0845/01_Example/xiaozhi-esp32) 示例。

## 安全

本仓库不保存任何密钥，photoframe 也不实现鉴权。不要提交 Wi-Fi 密码、配对凭据、NVS 镜像或设备备份。

## 许可

本仓库的原创代码使用 [MIT 许可](LICENSE)。取自 Waveshare 示例的部分保留其 MIT 声明，见 `LICENSES/Waveshare-PhotoPainter-MIT.txt`。
