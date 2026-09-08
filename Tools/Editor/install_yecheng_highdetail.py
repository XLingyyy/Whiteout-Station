"""Import, animate and install the baked high-detail character in the station."""
from pathlib import Path
import runpy
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve().parent
unreal.EditorLoadingAndSavingUtils.load_map('/Game/WindStation/World/MVP_StationMap')
for stage in ['import','animate','install']:
    runpy.run_path(str(ROOT/f'Tools/Editor/{stage}_yecheng_original.py'),
                   init_globals={'YECHENG_VARIANT':'HighDetail'})
unreal.log('YECHENG_HIGHDETAIL_INSTALL_COMPLETE')
unreal.SystemLibrary.quit_editor()
