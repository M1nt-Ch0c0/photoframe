# 无显示生命周期验收应用

使用与相框相同的 ABI 2。START 只登记 1 秒定时器并报告就绪；TIMER 递增计数并将 4 字节值存入自己的格式 1 RAM 缓存，同时调用单调/墙上时间服务；NETWORK_CHANGED 正常完成；STOP 取消定时器。没有 PNG、屏幕或联网要求。

在此目录用固定 IDF 执行 `idf.py -B build-runtime -DIDF_TARGET=esp32s3 build`。只使用 `build-runtime/lifecycle.app.elf`，不要刷普通 `.bin`。用宿主工具 `package --app lifecycle`、stage、activate 安装。状态应显示 payload_abi=2、runtime_ready=true、confirmed=true、inputs=0；定时运行后缓存增加 4 字节。切回其他应用后不再收到旧定时事件；测试结束可 `remove --app lifecycle`，释放 bank 和缓存。

此例用于验证通用运行模型，屏幕保留旧图是正常现象。它不是完整时钟，也没有新增授时协议。

维护验收可在独立目录使用 `-DLIFECYCLE_STALL_ON_BOOT=ON` 构建受控故障版本：它在 generation=1 的 START 内故意卡住，用于测量宿主看门狗与 previous 回退。只应在已有正常 lifecycle active 版本、完整备份且明确执行恢复测试时使用；先在已运行设备上激活（代次大于 1），再复位。正常应用构建默认关闭此选项。
