"""Crea subclases Blueprint vacías de las clases C++ para asignar arte final sin tocar código.

Regla del proyecto (CLAUDE.md): los Blueprints solo asignan meshes, sonidos, efectos y valores.
Toda la lógica está en C++. Mientras no haya arte, NO hacen falta: el juego usa las clases C++
con placeholders. Cuando exista el SK_Mechanic, asigna BP_MechanicCharacter en
Project Settings > How To Mechanic > Mechanic Character Class.

Uso:  py Tools/Scripts/create_blueprint_subclasses.py
"""

import unreal

import htm_editor_utils as htm

PACKAGE = "/Game/Blueprints"

# (Blueprint, clase C++ padre, subcarpeta)
BLUEPRINTS = [
    ("BP_MechanicCharacter", "MechanicCharacter", "Character"),
    ("BP_ModularCar", "ModularCar", "Vehicle"),
    ("BP_CarPart", "CarPart", "Parts"),
    ("BP_Tool", "Tool", "Tools"),
    ("BP_Jack", "Jack", "Tools"),
    ("BP_HydraulicLift", "HydraulicLift", "Tools"),
    ("BP_EngineCrane", "EngineCrane", "Tools"),
    ("BP_CustomerNPC", "CustomerNPC", "Customers"),
    ("BP_GrabbableProp", "GrabbableActor", "Props"),
]


def create_blueprint(name, parent_name, folder):
    package = "{}/{}".format(PACKAGE, folder)
    path = "{}/{}".format(package, name)
    if htm.asset_exists(path):
        htm.log("{} ya existe".format(name))
        return True
    parent = htm.load_cpp_class(parent_name)
    if parent is None:
        return False
    htm.ensure_directory(package)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    blueprint = htm.asset_tools().create_asset(name, package, unreal.Blueprint, factory)
    if blueprint is None:
        htm.error("No se pudo crear " + path)
        return False
    htm.save(blueprint)
    htm.log("{} creado (padre {})".format(name, parent_name))
    return True


def main():
    return all([create_blueprint(*entry) for entry in BLUEPRINTS])


if __name__ == "__main__":
    main()
