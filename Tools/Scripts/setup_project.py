"""Prepara el proyecto después de compilar por primera vez. Idempotente: se puede repetir.

  1. DT_* desde Content/Data/*.json         (import_data_tables.py)
  2. DA_Tuning                              (create_tuning_asset.py)
  3. M_Master y M_PP_Outline                (create_master_material.py)
  4. L_MainMenu y L_Workshop                (create_maps.py)
  5. Subclases BP_ vacías (opcional)        (create_blueprint_subclasses.py, con --blueprints)

Uso en el editor:  py Tools/Scripts/setup_project.py
                   py Tools/Scripts/setup_project.py --blueprints
Desde línea de comandos (sin abrir el editor):
  UnrealEditor-Cmd.exe HowToMechanic.uproject -run=pythonscript -script="Tools/Scripts/setup_project.py"
"""

import sys

import htm_editor_utils as htm
import create_maps
import create_master_material
import create_tuning_asset
import import_data_tables


def main(with_blueprints=False):
    steps = [
        ("Tablas de datos", import_data_tables.main),
        ("Tuning", create_tuning_asset.main),
        ("Materiales", create_master_material.main),
        ("Mapas", create_maps.main),
    ]
    if with_blueprints:
        import create_blueprint_subclasses

        steps.append(("Blueprints", create_blueprint_subclasses.main))

    failed = []
    for name, step in steps:
        htm.log("=== {} ===".format(name))
        try:
            if not step():
                failed.append(name)
        except Exception as exc:  # un paso roto no debe impedir los demás
            htm.error("{}: {}".format(name, exc))
            failed.append(name)

    if failed:
        htm.error("Setup incompleto. Fallaron: {}".format(", ".join(failed)))
    else:
        htm.log("Setup completo. Reinicia el editor para que DA_Tuning y los DT_* se usen en lugar del JSON.")
    return not failed


if __name__ == "__main__":
    main("--blueprints" in sys.argv)
