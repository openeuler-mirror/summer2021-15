# Summer2021-No.15 开发iso刻录DDE桌面应用

#### 介绍
https://gitee.com/openeuler-competition/summer-2021/issues/I3E9CG

具备将可启动iso镜像刻录至u盘的功能，另外支持U盘存储空间还原和刻录校验功能。

#### 软件架构
使用C++进行程序的编写，QT5框架进行软件前端的编写，可在Linux中运行。

![UI](screenshot/ui.png)

识别U盘设备直接调用Linux提供的接口`libudev`。=> [usbdevice.cpp](usbdevice.cpp)

写入镜像使用QIODevice类，然后使用QFile进行镜像的写入操作。=> [imagewriter.cpp](imagewriter.cpp)

清除将使用`mkfs`直接调用命令进行格式化的形式来实现。=> [mainwindow.cpp](mainwindow.cpp)

验证则直接使用shell脚本进行实现，通过UI调用。=> [verifyimagewriter](verifyimagewriter)

#### 安装教程

```bash
qmake-qt5 ImageWriter.pro
sudo make install
```

#### 使用说明

1. 首先选择好源镜像文件和将写入此镜像的目标U盘。
2. 点击**打开**图标，将显示**打开文件**对话框，可以在其中选择镜像文件。
3. 然后可以从相应的下拉列表中选择目标设备。列表应会隔一段时间自动更新，当插入或删除USB设备时，等一会儿会自动显示，也可以手动点击**刷新**图标。
4. 随后可以点击**写入**按钮，在进一步确认后，写入将开始，显示进度条和**取消**按钮，如果用户决定**取消**，则停止操作。
5. 如果要还原U盘设备，请点击**清除**按钮。它将从磁盘中删除分区数据，并且您将能够将其格式化为FAT32以达到最大容量。
6. 要验证写入的U盘的完整性，请点击**验证**按钮。这将比较ISO文件和U盘的数据散列号。

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request
