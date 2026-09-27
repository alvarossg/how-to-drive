"""Crea /Game/Data/DA_Tuning (UHTMTuningData) con los valores por defecto de C++.

Todo el tuning del caos se ajusta después en ese asset, sin tocar código.
Uso:  py Tools/Scripts/create_tuning_asset.py
"""

import unreal

import htm_editor_utils as htm

ASSET_PATH = "/Game/Data/DA_Tuning"


def main():
    htm.ensure_directory("/Game/Data")
    if htm.asset_exists(ASSET_PATH):
        htm.log("DA_Tuning ya existe (no se sobrescribe: puede tener ajustes de playtest)")
        return True

    tuning_class = htm.load_cpp_class("HTMTuningData")
    if tuning_class is None:
        return False

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", tuning_class)
    asset = htm.asset_tools().create_asset("DA_Tuning", "/Game/Data", tuning_class, factory)
    if asset is None:
        htm.error("No se pudo crear DA_Tuning")
        return False
    htm.save(asset)
    htm.log("DA_Tuning creado con los valores por defecto")
    return True


if __name__ == "__main__":
    main()
