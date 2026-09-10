# v1.7 构建与操作

产品版本 1.7.0，UIRevision=1，TutorialVersion=1。玩法规则沿用 v1.6 schema 8，AI 配置沿用 schema 9。工程使用 UE 5.8，直接在 main 开发。

## 运行与教程

打开 `WhiteoutStation/WhiteoutStation.uproject`，默认地图为 `/Game/WindStation/World/MVP_StationMap`。候选包构建在独立目录 `Artifacts/WhiteoutStation-v1.7-Win64-20260910-candidate2/Windows/`，实际构建与验收状态见实施和发行记录。

推进开场后，首次新局在安全可操作点显示教程。鼠标左键或右方向键继续，左方向键返回；Esc 打开跳过确认。教程期间游戏暂停，游戏操作不会穿透。末页完成后恢复控制。H 生存手册内可以重看，Esc 返回原手册位置。

完成或跳过后不再强制显示。中途退出保留阅读页。加载旧版游戏进度时不插入教程，只在安全点提示手册入口。图片全部随包提供，离线可读。

开发控制台命令 `Whiteout.ResetTutorial` 仅重置教程阅读记录。请在教程关闭时使用；它不删除游戏存档或重置其他设置。

## 存档与回滚

新槽为 `WhiteoutStation_Autosave_v1_7`，写入 UE 原生序列化内容及 CRC32 校验尾部。读取前验证完整字节，避免损坏数据进入反序列化。校验失败时不隐式选择旧槽，界面提供显式读取旧备份的选择。

旧槽 v1.6 至 v1.1 保持原格式、只读，迁移结果写入 v1.7 槽。旧档不会获得额外 AP 或对话轮次。未发行的早期 v1.7 开发夹具存档若没有校验尾部，会按无效新槽拒绝。

保留已验证的 v1.6 包及其旧槽；需要回滚时使用该包和旧槽。v1.7 新增进度不会自动降级写回 v1.6。不要清空用户 Saved 目录。

## 编译和候选打包

```powershell
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' WhiteoutStationEditor Win64 Development '-Project=G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' -WaitMutex -NoHotReloadFromIDE
& 'G:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun '-project=G:\Whiteout Station\WhiteoutStation\WhiteoutStation.uproject' -noP4 -nocompileeditor -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive '-archivedirectory=G:\Whiteout Station\Artifacts\WhiteoutStation-v1.7-Win64-20260910-candidate2'
```

后续候选使用新的目录名。禁止覆盖现用 v1.6 包。只有相同源码的 Editor 编译已通过时使用 `-nocompileeditor`。

## 定向验证入口

```powershell
python -X utf8 Tools/Release/validate_source_v17.py
python -X utf8 Tools/Capture/validate_tutorial_assets_v17.py
python -X utf8 Tools/Release/scan_secrets.py
python -X utf8 Tools/Capture/run_tutorial_v17.py --mode probe --performance --label _verified
python -X utf8 Tools/Capture/run_tutorial_v17.py --mode probe --layout-only --width 1280 --height 720 --scale 1.5 --label _720_150
python -X utf8 Tools/Release/run_v15_authored_routes.py --exe 'Artifacts/WhiteoutStation-v1.7-Win64-20260910-candidate2/Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe' --output Artifacts/v1.7-evidence/shipping-routes --rebalance
```

`run_tutorial_v17.py --exe` 可以指定真正的候选启动器或 Shipping 可执行文件。每次创建独立 UserDir；不会使用玩家存档。测试通过 Slate 原生事件驱动，OS 鼠标、输入法和真人理解验收需要独立记录。`--layout-only` 只运行五页与释放检查，不重复 30 次开关及存档用例。

配图源帧、原样裁片、几何及哈希清单位于 `SourceAssets/UI/v17/Tutorial/` 和 `docs/QA/v1.7_tutorial_assets.json`。导入入口为 `Tools/Editor/import_tutorial_v17.py`。任何配图更新都应重新采集并记录来源，不能修改图片中的实际状态文字。
