# 独立 ABI 2 color-test 应用

START 自主生成黑、白、黄、红、蓝、绿六条竖带。`COLOR_TEST_REVERSE=ON` 构建相反顺序，便于观察版本变化。应用只依赖 SDK、malloc/free 和宿主显示服务，不解码 PNG、不打包硬件帧、不自建任务；STOP 不刷新。向它推 PNG 会被宿主拒绝为 415。

在固定 IDF 环境从本目录执行：

```bash
idf.py -B build-chart -DIDF_TARGET=esp32s3 build
# 可选第二版本，独立目录
idf.py -B build-chart-reverse -DIDF_TARGET=esp32s3 -DCOLOR_TEST_REVERSE=ON build
```

只部署 `build-chart/color_test.app.elf`，普通 `.bin` 是构建占位固件，不能刷入设备。宿主必须支持 ABI 2。版本号选择比本机已安装版本清晰可区分的值：

```bash
python3 ../../../photopainter-host/tools/module.py package --app color-test --version 3 --input build-chart/color_test.app.elf --output /tmp/color-test.pkg
python3 ../../../photopainter-host/tools/module.py stage --app color-test --url http://DEVICE_IP --input /tmp/color-test.pkg
python3 ../../../photopainter-host/tools/module.py activate --app color-test --url http://DEVICE_IP
python3 ../../../photopainter-host/tools/module.py status --url http://DEVICE_IP
```

activate 本身等待首屏与确认，无需推图。切换已安装的 active 版本可用 GPIO 4 短按或 CLI switch；pending 更新须显式 activate。验收前停止自动推图，结束后切回 photoframe、推一次有效额度图并检查画面，再恢复服务。容量、回退和按键忙时行为见宿主 `docs-runtime-v2.md`。
