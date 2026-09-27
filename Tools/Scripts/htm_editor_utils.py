"""Utilidades comunes para los scripts de editor de How to Mechanic.

Se ejecutan dentro del editor de Unreal (Python Editor Script Plugin):
    Tools > Execute Python Script...  o  consola "py Tools/Scripts/setup_project.py"
"""

import os

import unreal

MODULE = "/Script/HowToMechanic"


def log(msg):
    unreal.log("[HTM] " + msg)


def warn(msg):
    unreal.log_warning("[HTM] " + msg)


def error(msg):
    unreal.log_error("[HTM] " + msg)


def project_dir():
    return os.path.abspath(unreal.Paths.project_dir())


def content_data_dir():
    return os.path.join(os.path.abspath(unreal.Paths.project_content_dir()), "Data")


def asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def load_cpp_class(name):
    """Clase C++ del módulo (sin prefijo A/U): load_cpp_class("MechanicCharacter")."""
    cls = unreal.load_class(None, "{}.{}".format(MODULE, name))
    if cls is None:
        error("No se encuentra la clase {}.{} (¿está compilado el módulo?)".format(MODULE, name))
    return cls


def load_cpp_struct(name):
    """Struct C++ del módulo (sin prefijo F): load_cpp_struct("PartDefinitionRow")."""
    struct = unreal.load_object(None, "{}.{}".format(MODULE, name))
    if struct is None:
        error("No se encuentra el struct {}.{} (¿está compilado el módulo?)".format(MODULE, name))
    return struct


def save(asset):
    if asset is not None:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def hex_to_linear(hex_color, alpha=1.0):
    """Color de la paleta (sRGB hex) a unreal.LinearColor."""
    hex_color = hex_color.lstrip("#")

    def channel(value):
        value = int(value, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4

    return unreal.LinearColor(channel(hex_color[0:2]), channel(hex_color[2:4]), channel(hex_color[4:6]), alpha)


# Paleta oficial (docs/ART_DIRECTION.md §3).
PALETTE = {
    "WallMint": "8FC1A9",
    "FloorWarmGrey": "B8B0A4",
    "SafetyOrange": "FF8A3D",
    "SignalYellow": "FFD23F",
    "ToolRed": "E5484D",
    "ToolBlue": "3E7CB1",
    "Rust": "B5651D",
    "CarSky": "9ED8F0",
    "Ink": "2F2A38",
    "White": "FAF7F0",
    "Dirt": "A07A52",
    "Water": "6FB8D9",
}
