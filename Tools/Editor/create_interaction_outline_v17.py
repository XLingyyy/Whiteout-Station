"""Build the focused-object outline without modifying the user's map or mesh materials."""
import unreal

PATH = '/Game/WindStation/UI/v17'
NAME = 'M_WS_InteractionOutline_A'
CODE = r'''
float2 uv = GetDefaultSceneTextureUV(Parameters, 25);
float2 pixel = View.BufferSizeAndInvSize.zw * 2.0;
float center = abs(Stencil.r - 241.0) < 0.5 ? 1.0 : 0.0;
float edge = 0.0;
// Eight neighboring stencil samples keep the outline legible at diagonal edges.
for (int y = -1; y <= 1; ++y)
{
    for (int x = -1; x <= 1; ++x)
    {
        float2 sampleUV = uv + float2(x, y) * pixel;
        float selected = abs(SceneTextureLookup(sampleUV, 25, false).r - 241.0) < 0.5 ? 1.0 : 0.0;
        float customDepth = SceneTextureLookup(sampleUV, 13, false).r;
        float sceneDepth = SceneTextureLookup(sampleUV, 1, false).r;
        edge = max(edge, selected * step(customDepth, sceneDepth + 1.0));
    }
}
return lerp(SceneColor.rgb, float3(0.95, 0.97, 0.96), edge * (1.0 - center));
'''

def main():
    lib = unreal.MaterialEditingLibrary
    material = unreal.load_asset(PATH + '/' + NAME)
    if material:
        lib.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property('blendable_location', unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    color = lib.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -600, 0)
    color.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    stencil = lib.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -600, 200)
    stencil.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -200, 0)
    custom.set_editor_property('code', CODE)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ['SceneColor', 'Stencil']:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    custom.set_editor_property('inputs', inputs)
    lib.connect_material_expressions(color, 'Color', custom, 'SceneColor')
    lib.connect_material_expressions(stencil, 'Color', custom, 'Stencil')
    lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log('V17 interaction outline material saved')

if __name__ == '__main__': main()
