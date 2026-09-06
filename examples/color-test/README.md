# 独立 color-test 应用

该应用有独立入口、独立 CMake 工程和独立 `.app.elf`，与原 `photoframe` 应用共用本仓库的 PNG 校验与面板驱动源码。它接收一个符合原输入契约的 PNG，完整校验后显示六条色带；原 photoframe 则显示 PNG 本身。因此可验证同一宿主上两个不同应用分别安装、选择和更新。

两个构建版本输出不同：v1 从左到右为黑、白、黄、红、蓝、绿；v2 顺序相反。二者均在最终 POWER_OFF 等待完成后才报告成功。不接受坏图，不创建后台任务；调用结束释放所有资源。

从 photoframe 仓库根目录，在固定 ESP-IDF 环境执行：

```bash
idf.py -C examples/color-test -B build-color-test-v1 -DIDF_TARGET=esp32s3 build
idf.py -C examples/color-test -B build-color-test-v2 -DIDF_TARGET=esp32s3 -DCOLOR_TEST_REVERSE=ON build
python3 -m unittest discover -s examples/color-test -p 'test_*.py'
```

只部署 `build-color-test-v1/color_test.app.elf` 和 v2 的对应 ELF。构建过程同时产生的普通 `.bin` 不用于设备，不能刷入现有宿主。两个 ELF 都通过固定宿主桥接 ABI 1 和 Registry elf_loader 1.3.3 的默认 libc 符号运行；无需修改宿主 ABI 或 Registry 源码。

使用兄弟 host 仓库的工具：

```bash
python3 ../photopainter-host/tools/module.py package --app color-test --version 1 \
  --input build-color-test-v1/color_test.app.elf --output /tmp/color-test-v1.pkg
python3 ../photopainter-host/tools/module.py stage --app color-test --url http://DEVICE_IP --input /tmp/color-test-v1.pkg
python3 ../photopainter-host/tools/module.py activate --app color-test --url http://DEVICE_IP
python3 ../photopainter-host/tools/module.py push --app color-test --url http://DEVICE_IP --input /path/to/valid-frame.png
```

成功刷屏后读取 `status`，应看到 color-test 的 active=0、selected 指向其 bank。确认屏幕颜色与方向。v2 使用 `--version 2` 与 v2 ELF，再 stage/activate/push；此时该应用 A/B 应为 [1,2]，原 photoframe 的 A/B 不变。`rollback --app color-test` 可切回 v1。

测试前停止自动推图；切回 `photoframe` 并完成一次成功推图、检查画面后再恢复常驻服务。详细状态和恢复语义见 [宿主多应用指南](https://github.com/M1nt-Ch0c0/photopainter-host/blob/codex/multi-wifi-apps/docs-multi-apps.md)。
