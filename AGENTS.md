# Whiteout Station 工作约定

- 默认直接在 `main` 分支修改、提交和推送；除非用户明确更改要求，不创建额外分支或 Git 工作树。
- 地图 `WhiteoutStation/Content/WindStation/World/MVP_StationMap.umap`、NPC 模型及其材质属于用户编辑资产。修改前记录哈希，发布前复核，避免覆盖用户摆场和换模结果。
- 旧版本文件、构建缓存和工作树仅在最新版完成打包与回归验证后清理；删除前确认绝对路径和依赖关系。
