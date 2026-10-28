# 配对 v2 应用兼容性：尚未实机验证

2026-10-29 夜间仅做离线工作，没有访问蓝牙、串口或设备，没有推图。

photoframe 继续使用 ABI 2.2 和 PFI1 v1。配对提示、GPIO4、NVS owner 和撤权均由 host 实现，本轮未改变业务 APP 源码或 SDK 副本，不要求为配对重新安装 v20。

补齐原生测试的 ABI 2.2 缺口：

- `test_ble_image.c` 的上下文/manifest 断言改为实际 2.2；用可控 CoC 接收/背压模型连接真实 `entry.c`、`ble_image.c` 和 libpng。
- 覆盖大于 GATT 上限的 CoC 块、回复背压时不继续消耗 RX、成功 CHUNK 无逐块 ACK、GATT 不接管 CoC 接收、重复 COMMIT 不再次调用显示、断线后原回执查询、旧 session 不消耗数据及未提交输入清理。
- 首屏/缓存的 Python 测试只隔离 BLE 协议层；收图行为由上述真实业务测试覆盖，不用空桩冒充协议验收。

离线验证命令：

```sh
cmake -S tests -B tests/build-night
cmake --build tests/build-night
ctest --test-dir tests/build-night --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
python3 -m unittest discover -s examples/color-test -p 'test_*.py'
```

结果：3/3 CTest、4/4 入口策略测试和 3/3 色条示例测试通过。固定 ESP-IDF `5e6f53cdb31fe5708eae3f55af9737be2822db22` 的 app/SO 构建结果及最终产物摘要见共享晨间清单和外部 manifest；编译器输出的 flash 示例没有执行。

所有测试显示回调均为替身。配对、按键、重启持久化、换绑拒绝旧 owner、真实显示回执与画面方向/颜色仍须早上验收，不能用这里的绿色测试代替。
