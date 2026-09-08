"""Resolve VRM material differences explicitly in UE without changing V10."""
import unreal

DEST = '/Game/WindStation/Art/AnimeNPC/YeChengWinter'
mesh = unreal.load_asset(DEST + '/SK_YeCheng_Winter')
old = unreal.load_asset('/Game/WindStation/Art/AnimeNPC/YeChengV10/SK_YeCheng_NoanoaHair_RefinedFace_v10')
original = {str(m.material_slot_name): m.material_interface for m in old.materials}
colors = {
    'YC_SnowShell': ((.73,.76,.82),0,.7),
    'YC_SlateBlueFabric': ((.075,.115,.23),0,.85),
    'YC_CharcoalCloth': ((.018,.023,.033),0,.85),
    'YC_BlackLeather': ((.012,.016,.022),0,.48),
    'YC_BrushedSteel': ((.34,.39,.46),.8,.3),
    'YC_IceFur': ((.66,.70,.78),0,.95),
    'YC_BadgeWhite': ((.9,.93,.97),0,.65),
}
materials = list(mesh.materials)
# VRM4U's Assimp pass drops unused street-outfit materials. Older exports kept
# those three slots in the VRM material table, offsetting every later section.
unused = {'F00_008_01_Tops_01_CLOTH','F00_001_01_Bottoms_01_CLOTH','F00_006_01_Shoes_01_CLOTH'}
materials = [m for m in materials if str(m.material_slot_name) not in unused]
for i,slot in enumerate(materials):
    name = str(slot.material_slot_name)
    if name in original:
        slot.material_interface = original[name]
    elif name in colors:
        color,metal,rough = colors[name]
        path = DEST + '/M_' + name
        m = unreal.load_asset(path)
        if not m:
            m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_'+name,DEST,unreal.Material,unreal.MaterialFactoryNew())
            m.set_editor_property('two_sided', True)
            e = unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-300,0)
            e.set_editor_property('constant',unreal.LinearColor(*color,1))
            unreal.MaterialEditingLibrary.connect_material_property(e,'',unreal.MaterialProperty.MP_BASE_COLOR)
            for prop,val,y in [(unreal.MaterialProperty.MP_METALLIC,metal,140),(unreal.MaterialProperty.MP_ROUGHNESS,rough,260)]:
                e=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant,-300,y)
                e.set_editor_property('r',val)
                unreal.MaterialEditingLibrary.connect_material_property(e,'',prop)
            unreal.MaterialEditingLibrary.recompile_material(m)
            unreal.EditorAssetLibrary.save_loaded_asset(m)
        slot.material_interface = m
    else:
        raise RuntimeError('Unmapped material slot '+name)
mesh.set_editor_property('materials',materials)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
unreal.log('YECHENG_WINTER_MATERIALS_COMPLETE')
