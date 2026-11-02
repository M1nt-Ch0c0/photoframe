# ble-echo 示例

ble-echo 演示应用怎样注册自己的蓝牙 GATT 服务：客户端写入一个值，应用把它原样放进同一个特征，客户端再读回来。它不显示画面。

这个示例使用 `APP_MANIFEST_BLE`，声明 ABI 2.1，需要支持 ABI 2.1 或更新的宿主。GATT 服务的接口规则见 [应用接口参考](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/05-app-api-reference.md)，三种应用通信方式的比较见 [蓝牙通信](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/06-bluetooth.md#8-应用通信的三种方式)。

## 行为

| 事件 | 处理 |
|---|---|
| START | 注册服务，把特征 0 的初始值设为 `ready` |
| BLE 写入 | 把写入的值设为该特征的新值 |
| BLE 出错 | 注销服务 |
| STOP | 注销服务 |
| INPUT | 返回 `APP_ERR_UNSUPPORTED` |

服务 UUID 为 `a4fe1000-1536-4c88-a2ce-32ecdaac0471`，只有一个特征：

| 序号 | UUID | 属性 | 最大长度 |
|---:|---|---|---:|
| 0 | `a4fe1001-1536-4c88-a2ce-32ecdaac0471` | 读、写、通知 | 128 字节 |

蓝牙连接、鉴权和事件调度都由宿主负责。客户端写特征也要通过宿主的签名命令，不能直接写裸数据。

## 构建和打包

在固定版本的 ESP-IDF 环境里，从本目录构建：

```sh
idf.py -B build-app-ble -DIDF_TARGET=esp32s3 build
```

产物是 `build-app-ble/ble_echo.app.elf`。然后在兄弟目录 `photopainter-host` 里打包：

```sh
python3 tools/module.py package --version 1 \
  --input ../photoframe/examples/ble-echo/build-app-ble/ble_echo.app.elf \
  --output /tmp/ble-echo-1.pkg
```

## 安装和试用

用 `ai-quota-frame` 命令行安装并激活，然后写入、读回：

```sh
ai-quota-frame ble install    --file /tmp/ble-echo-1.pkg
ai-quota-frame ble activate   --app ble-echo
ai-quota-frame ble app-write  --app ble-echo --text hello
ai-quota-frame ble app-read   --app ble-echo
ai-quota-frame ble switch     --app photoframe
```

`app-read` 应读到 `hello`。切回 photoframe 后，宿主撤销 ble-echo 的服务，再访问它会被拒绝。

也可以用网页插件的开发者工具上传并激活 `.pkg`。两种方式的完整说明见 [编写一个应用](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/04-writing-an-app.md#第五步装到设备上)。

## 测试

原生测试 `test_entry.c` 在电脑上调用真实的应用入口，依次模拟 START、写入、连接、断开和 STOP，检查特征值和完成结果。它属于 photoframe 根目录的测试工程，在仓库根目录运行：

```sh
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```
