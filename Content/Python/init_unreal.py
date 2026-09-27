"""Se ejecuta al abrir el editor (Content/Python/init_unreal.py).

Si faltan los assets generados (primer arranque tras clonar), lanza Tools/Scripts/setup_project.py.
Para desactivarlo: variable de entorno HTM_NO_AUTO_SETUP=1.
"""

import os
import sys

import unreal

_scripts = os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Tools", "Scripts")
if _scripts not in sys.path:
    sys.path.append(_scripts)

_handle = None


def _run_setup_once(_delta_seconds):
    """Primer tick del editor: ya está todo inicializado para crear assets y mapas."""
    global _handle
    unreal.unregister_slate_post_tick_callback(_handle)
    if unreal.EditorAssetLibrary.does_asset_exist("/Game/Data/DT_Parts"):
        return
    unreal.log("[HTM] Primer arranque: generando datos, materiales y mapas (Tools/Scripts/setup_project.py)")
    try:
        import setup_project

        setup_project.main()
    except Exception as exc:
        unreal.log_error("[HTM] El setup automático falló: {}. Ejecútalo a mano: py Tools/Scripts/setup_project.py".format(exc))


if os.environ.get("HTM_NO_AUTO_SETUP") != "1" and not unreal.EditorAssetLibrary.does_asset_exist("/Game/Data/DT_Parts"):
    _handle = unreal.register_slate_post_tick_callback(_run_setup_once)
