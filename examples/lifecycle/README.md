# lifecycle 示例

lifecycle 是一个不显示画面的最小应用，演示事件驱动的生命周期：启动时设一个定时器，每次定时器到期计数一次并写进缓存，停止时取消定时器。屏幕会一直保留之前的画面，这是正常的。

这个示例使用 `APP_MANIFEST`，声明 ABI 2.0，不需要显示首屏，也不接收图片。[编写一个应用](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/04-writing-an-app.md) 的教程以它的工程结构为蓝本。

## 行为

| 事件 | 处理 |
|---|---|
| START | 计数清零，设置 1 秒的定时器 1，报告就绪 |
| TIMER | 计数加 1；读取单调时间和墙上时间；把 4 字节的计数写进格式版本 1 的缓存 |
| STOP | 取消定时器 |
| INPUT | 返回 `APP_ERR_UNSUPPORTED` |
| 其他事件 | 直接完成 |

## 构建、打包和安装

在固定版本的 ESP-IDF 环境里，从本目录构建：

```sh
idf.py -B build-runtime -DIDF_TARGET=esp32s3 build
```

产物是 `build-runtime/lifecycle.app.elf`。在兄弟目录 `photopainter-host` 里打包，再安装并激活：

```sh
python3 tools/module.py package --version 1 \
  --input ../photoframe/examples/lifecycle/build-runtime/lifecycle.app.elf \
  --output /tmp/lifecycle-1.pkg

ai-quota-frame ble install  --file /tmp/lifecycle-1.pkg
ai-quota-frame ble activate --app lifecycle
ai-quota-frame ble status
```

激活后，`status` 输出里应看到 `payload_abi` 为 2、`runtime_ready` 和 `confirmed` 为 `true`，运行一段时间后 `cache_bytes` 包含这 4 字节缓存。切到别的应用后，lifecycle 不再收到定时器事件。不需要时用 `ai-quota-frame ble remove --app lifecycle` 删除，释放它的槽位和缓存。

也可以用网页插件的开发者工具安装。完整说明见 [编写一个应用](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/04-writing-an-app.md#第五步装到设备上)。

## 启动保护测试版本

构建选项 `LIFECYCLE_STALL_ON_BOOT=ON` 生成一个故意出错的版本：相框重启后第一次加载它（加载代次 `generation` 为 1）时，START 永远不返回。它用来测试宿主的看门狗和启动保护能否回退到上一个版本，默认关闭。

```sh
idf.py -B build-stall -DIDF_TARGET=esp32s3 -DLIFECYCLE_STALL_ON_BOOT=ON build
```

使用步骤：

1. 先装好一个正常的 lifecycle 版本并确认，同时备份设备，见 [维护与排错](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/08-maintenance.md#备份)。
2. 在相框已经运行的状态下安装并激活故障版本。这时代次大于 1，START 正常返回。
3. 重启相框。故障版本在启动时卡住，宿主应回退到之前的版本。

启动保护的规则见 [设备是怎么工作的](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/03-how-the-device-works.md#启动保护)。
