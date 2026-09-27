"""Crea o actualiza los DT_* de /Game/Data a partir de Content/Data/*.json.

Uso (en el editor):  py Tools/Scripts/import_data_tables.py
Si cambias un struct F*Row en C++, compila, actualiza el JSON y vuelve a ejecutar este script.
"""

import os

import unreal

import htm_editor_utils as htm

# JSON -> (asset, struct de fila en C++)
TABLES = [
    ("Parts", "DT_Parts", "PartDefinitionRow"),
    ("CarModels", "DT_CarModels", "CarModelRow"),
    ("Jobs", "DT_Jobs", "JobDefinitionRow"),
    ("Traits", "DT_Traits", "CarTraitRow"),
    ("Cosmetics", "DT_Cosmetics", "CosmeticRow"),
    ("WorkshopLevels", "DT_WorkshopLevels", "WorkshopLevelRow"),
    ("BuyerProfiles", "DT_BuyerProfiles", "BuyerProfileRow"),
    ("CarNames", "DT_CarNames", "CarNameRow"),
]

PACKAGE = "/Game/Data"


def import_table(json_name, asset_name, struct_name):
    json_path = os.path.join(htm.content_data_dir(), json_name + ".json")
    if not os.path.isfile(json_path):
        htm.error("Falta {}".format(json_path))
        return False

    struct = htm.load_cpp_struct(struct_name)
    if struct is None:
        return False

    asset_path = "{}/{}".format(PACKAGE, asset_name)
    table = None
    if htm.asset_exists(asset_path):
        table = unreal.EditorAssetLibrary.load_asset(asset_path)
        if table.get_editor_property("row_struct") != struct:
            htm.warn("{} tenía otro RowStruct: se recrea".format(asset_name))
            unreal.EditorAssetLibrary.delete_asset(asset_path)
            table = None

    if table is None:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", struct)
        table = htm.asset_tools().create_asset(asset_name, PACKAGE, unreal.DataTable, factory)
        if table is None:
            htm.error("No se pudo crear {}".format(asset_path))
            return False

    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_file(table, json_path)
    if not ok:
        htm.error("Error importando {} (mira el Output Log: campo o enum mal escrito)".format(json_path))
        return False

    htm.save(table)
    rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    htm.log("{}: {} filas".format(asset_name, len(rows)))
    return True


def main():
    htm.ensure_directory(PACKAGE)
    results = [import_table(*entry) for entry in TABLES]
    if all(results):
        htm.log("Tablas importadas correctamente")
    else:
        htm.error("Alguna tabla no se importó. Revisa los mensajes anteriores.")
    return all(results)


if __name__ == "__main__":
    main()
