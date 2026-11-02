# color-test 示例

color-test 是一个自己画屏的应用：启动时生成六条竖直色带并显示，不接收图片。它可以用来检查屏幕的六种颜色，也可以作为第二个应用，练习多个应用之间的安装、切换和回滚。

这个示例使用 `APP_MANIFEST`，声明 ABI 2.0，并声明需要显示首屏（`APP_REQUIRES_DISPLAY`）。

## 行为

| 事件 | 处理 |
|---|---|
| START | 从左到右生成黑、白、黄、红、蓝、绿六条竖带，调用宿主显示接口 |
| INPUT | 返回 `APP_ERR_UNSUPPORTED` |
| 其他事件 | 直接完成，不刷新屏幕 |

应用只用到 SDK、`malloc`/`free` 和宿主的显示服务。

## 构建

在固定版本的 ESP-IDF 环境里，从本目录构建：

```sh
idf.py -B build-chart -DIDF_TARGET=esp32s3 build
```

产物是 `build-chart/color_test.app.elf`。

构建选项 `COLOR_TEST_REVERSE=ON` 把色带顺序反过来。用它构建第二个版本，更新后一眼就能看出屏幕上是哪一版：

```sh
idf.py -B build-chart-reverse -DIDF_TARGET=esp32s3 -DCOLOR_TEST_REVERSE=ON build
```

## 打包和安装

在兄弟目录 `photopainter-host` 里打包：

```sh
python3 tools/module.py package --version 1 \
  --input ../photoframe/examples/color-test/build-chart/color_test.app.elf \
  --output /tmp/color-test-1.pkg
```

然后用 `ai-quota-frame` 命令行安装并激活：

```sh
ai-quota-frame ble install  --file /tmp/color-test-1.pkg
ai-quota-frame ble activate --app color-test
```

`activate` 会等到色条显示完成才返回。也可以用网页插件的开发者工具上传并激活。两种方式的完整说明见 [编写一个应用](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/04-writing-an-app.md#第五步装到设备上)。

装好后，短按相框的功能键可以在 color-test 和 photoframe 之间切换，也可以用 `ai-quota-frame ble switch --app photoframe` 切回相框。

## 测试

`test_entry.py` 在电脑上把 `main/entry.c` 编译成动态库，分别用两种色带顺序调用 START，检查每个像素的颜色和完成结果。在 photoframe 仓库根目录运行：

```sh
python3 -m unittest discover -s examples/color-test -p 'test_*.py'
```
