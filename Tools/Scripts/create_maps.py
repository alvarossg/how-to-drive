"""Crea los mapas mínimos:

  /Game/Maps/L_MainMenu  Vacío, GameMode = AHTMMenuGameMode (también llega por GameModeMapPrefixes).
  /Game/Maps/L_Workshop  AWorkshopBlockout + 4 PlayerStart. GameMode = AHTMGameMode.
                         El taller, la explanada, la zona de pruebas y el desguace se construyen
                         al empezar la partida (BeginPlay del blockout), no en el editor.

No sobrescribe mapas existentes (pueden tener trabajo de nivel encima).
Uso:  py Tools/Scripts/create_maps.py
"""

import unreal

import htm_editor_utils as htm

PACKAGE = "/Game/Maps"

# Mismas posiciones que AWorkshopBlockout::GetLayout(...).PlayerStarts.
PLAYER_STARTS = [(-150.0 - (i // 2) * 150.0, (120.0 if i % 2 else -120.0), 110.0) for i in range(4)]


def _level_subsystem():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def _actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _set_game_mode(class_name):
    cls = htm.load_cpp_class(class_name)
    world = _editor_world()
    if cls and world:
        world.get_world_settings().set_editor_property("default_game_mode", cls)


def create_main_menu():
    path = PACKAGE + "/L_MainMenu"
    if htm.asset_exists(path):
        htm.log("L_MainMenu ya existe")
        return True
    if not _level_subsystem().new_level(path):
        htm.error("No se pudo crear " + path)
        return False
    _set_game_mode("HTMMenuGameMode")
    _level_subsystem().save_current_level()
    htm.log("L_MainMenu creado")
    return True


def create_workshop():
    path = PACKAGE + "/L_Workshop"
    if htm.asset_exists(path):
        htm.log("L_Workshop ya existe")
        return True
    if not _level_subsystem().new_level(path):
        htm.error("No se pudo crear " + path)
        return False
    _set_game_mode("HTMGameMode")

    actors = _actor_subsystem()
    blockout_class = htm.load_cpp_class("WorkshopBlockout")
    if blockout_class:
        blockout = actors.spawn_actor_from_class(blockout_class, unreal.Vector(0, 0, 0))
        blockout.set_actor_label("WorkshopBlockout")
    for index, (x, y, z) in enumerate(PLAYER_STARTS):
        start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, y, z))
        start.set_actor_label("PlayerStart_{}".format(index + 1))

    _level_subsystem().save_current_level()
    htm.log("L_Workshop creado")
    return True


def main():
    htm.ensure_directory(PACKAGE)
    ok = create_main_menu() and create_workshop()
    # Deja abierto el taller para trabajar.
    if ok and htm.asset_exists(PACKAGE + "/L_Workshop"):
        _level_subsystem().load_level(PACKAGE + "/L_Workshop")
    return ok


if __name__ == "__main__":
    main()
