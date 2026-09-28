# -*- coding: utf-8 -*-
"""Create, update and read back the station kit's place assets (station-kit plan, tasks 3, 8, 13, 15, 17, 18),
and the one control asset of the ParkedHulls record check (task 17).

Run by the editor's own Python through the commandlet, never by a system Python:

    KIT_PLACES_GROUP=latent UnrealEditor-Cmd.exe <worktree>/StarForge.uproject -run=pythonscript \
        -script="<worktree>/scripts/station_kit_places.py" -unattended -nopause -nosplash -nullrhi -log

The group comes in an environment variable because the commandlet cuts -Script= at the first space
when the value is not quoted, and from Git Bash it never arrives quoted
(PythonScriptCommandlet.cpp 20-33).

Groups: probe, latent, berth, piercore, tie, parked0, parkedboth, badrecords, read. Every asset is written with
only the fields its place needs; the rest keep their class defaults, and a UDataAsset does not
serialise what equals its defaults -- so an asset made here reads, field for field, like one made by
hand with the same values. Every write is saved and read back, and the read-back is the line to
check, not the write.
"""
import os
import unreal

PLACES = "/Game/Places"


def cls(name):
    c = unreal.load_class(None, f"/Script/StarForge.{name}")
    if c is None:
        raise RuntimeError(f"no class /Script/StarForge.{name}: is the editor target of this tree built?")
    return c


def vec(v):
    return unreal.Vector(float(v[0]), float(v[1]), float(v[2]))


def carrier(class_name, name, loc=(0.0, 0.0, 0.0)):
    c = unreal.SFPlaceCarrier()
    c.set_editor_property("carrier_class", cls(class_name))
    c.set_editor_property("carrier_name", name)
    c.set_editor_property("location_in_place_metres", vec(loc))
    c.set_editor_property("rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    return c


def free_start(loc, occupant=(0.0, 0.0, 0.0)):
    s = unreal.SFShipStart()
    s.set_editor_property("mode", unreal.SFShipStartMode.FREE)
    s.set_editor_property("location_in_place_metres", vec(loc))
    s.set_editor_property("rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    s.set_editor_property("occupant_offset_ship_metres", vec(occupant))
    return s


def parked(hull, station, port):
    p = unreal.SFParkedHull()
    p.set_editor_property("hull_name", hull)
    p.set_editor_property("station_carrier_name", station)
    p.set_editor_property("port_index", int(port))
    return p


def make(asset_name, place_name, carriers, ship_start=None, parked_hulls=None):
    path = f"{PLACES}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls("SFPlaceDataAsset"))
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name, PLACES, cls("SFPlaceDataAsset"), factory)
    if asset is None:
        raise RuntimeError(f"{path} could not be created or loaded")
    asset.set_editor_property("place_name", place_name)
    asset.set_editor_property("carriers", list(carriers))
    if ship_start is not None:
        asset.set_editor_property("ship_start", ship_start)
    if parked_hulls is not None:
        asset.set_editor_property("parked_hulls", list(parked_hulls))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"{path} was not saved")
    unreal.log(f"KIT-ASSET made {path}")
    read(asset_name)


def fmt(v):
    return f"({v.x:.2f} {v.y:.2f} {v.z:.2f})"


def read(asset_name):
    path = f"{PLACES}/{asset_name}"
    asset = unreal.load_asset(path)
    if asset is None:
        unreal.log_error(f"KIT-ASSET read {path}: MISSING")
        return
    carriers = []
    for c in asset.get_editor_property("carriers"):
        k = c.get_editor_property("carrier_class")
        carriers.append(f"{k.get_name() if k else 'NO CLASS'} '{c.get_editor_property('carrier_name')}' "
                        f"at {fmt(c.get_editor_property('location_in_place_metres'))}")
    text = f"KIT-ASSET read {path}: place '{asset.get_editor_property('place_name')}', carriers [{'; '.join(carriers)}]"
    s = asset.get_editor_property("ship_start")
    text += (f", ship start {s.get_editor_property('mode')} at {fmt(s.get_editor_property('location_in_place_metres'))}"
             f", occupant {fmt(s.get_editor_property('occupant_offset_ship_metres'))}")
    try:
        hulls = asset.get_editor_property("parked_hulls")
        text += ", parked [" + "; ".join(
            f"'{h.get_editor_property('hull_name')}' at {h.get_editor_property('station_carrier_name')}"
            f".{h.get_editor_property('port_index')}" for h in hulls) + "]"
    except Exception:  # the field arrives with plan task 17
        text += ", parked (no such field yet)"
    unreal.log(text)


def probe():
    exists = unreal.EditorAssetLibrary.does_asset_exist(f"{PLACES}/DA_Place_OpenSpace")
    cls("SFPlaceDataAsset")
    unreal.log(f"KIT-ASSET probe: editor python ok, SFPlaceDataAsset found, DA_Place_OpenSpace exists {exists}")
    if not exists:
        raise RuntimeError("DA_Place_OpenSpace not found: the asset library cannot see /Game/Places")


# Spec 6.9: the rig station at the place's origin, the ship free with its +Y (hatch) side towards the
# station's -Y wall, the man 3.2 m towards the hatch wall and 1.0 m aside, in the ship's axes.
LATENT_OCCUPANT = (-1.0, 3.2, 0.0)

GROUPS = {
    "probe": lambda: probe(),
    "latent": lambda: (
        make("DA_Place_LatentF1Side", "LatentF1Side", [carrier("SFTestStation", "Station")],
             ship_start=free_start((5.40, -11.10, 0.00), LATENT_OCCUPANT)),
        make("DA_Place_LatentF1Roof", "LatentF1Roof", [carrier("SFTestStation", "Station")],
             ship_start=free_start((0.00, -11.10, 3.00), LATENT_OCCUPANT))),
    "berth": lambda: make("DA_Place_Berth", "Berth", [carrier("SFBerthStation", "Station")]),
    "piercore": lambda: make("DA_Place_PierCore", "PierCore", [carrier("SFPierCoreStation", "Station")]),
    # Spec 7.5: the mid-plane between Port0 and Port1 through the place's origin -- one number in the data.
    "tie": lambda: make("DA_Place_PierCoreTie", "PierCoreTie",
                        [carrier("SFPierCoreStation", "Station", (-30.0, 0.0, 0.0))]),
    "parked0": lambda: make("DA_Place_PierCoreParked0", "PierCoreParked0",
                            [carrier("SFPierCoreStation", "Station")],
                            parked_hulls=[parked("Parked", "Station", 0)]),
    # Spec 8.3, N4: both ports held, and the ship free at (0, -40, 0) m, 9.9 m clear of Parked0's hull.
    "parkedboth": lambda: make("DA_Place_PierCoreParkedBoth", "PierCoreParkedBoth",
                               [carrier("SFPierCoreStation", "Station")],
                               ship_start=free_start((0.0, -40.0, 0.0)),
                               parked_hulls=[parked("Parked0", "Station", 0), parked("Parked1", "Station", 1)]),
    # Plan task 17: the check of a ParkedHulls record, run rather than trusted. Record 0 is sound and must
    # be the only hull that spawns; each record after it fails exactly one check, in the order the rig
    # makes them -- no name, 'Ship', a carrier's name, an earlier record's name, no such carrier, no such
    # port, a port already held. A control, not a place of the kit: its run never goes into evidence.
    "badrecords": lambda: make("DA_Place_PierCoreBadRecords", "PierCoreBadRecords",
                               [carrier("SFPierCoreStation", "Station")],
                               parked_hulls=[parked("Parked0", "Station", 0),
                                             parked("", "Station", 1),
                                             parked("Ship", "Station", 1),
                                             parked("Station", "Station", 1),
                                             parked("Parked0", "Station", 1),
                                             parked("X", "Nope", 1),
                                             parked("Y", "Station", 5),
                                             parked("Parked1", "Station", 0)]),
    "read": lambda: [read(n) for n in ("DA_Place_OpenSpace", "DA_Place_LatentF1Side", "DA_Place_LatentF1Roof",
                                        "DA_Place_Berth", "DA_Place_PierCore", "DA_Place_PierCoreTie",
                                        "DA_Place_PierCoreParked0", "DA_Place_PierCoreParkedBoth",
                                        "DA_Place_PierCoreBadRecords")],
}

group = os.environ.get("KIT_PLACES_GROUP", "")
if group not in GROUPS:
    raise RuntimeError(f"KIT_PLACES_GROUP='{group}' is none of {sorted(GROUPS)}")
GROUPS[group]()
unreal.log(f"KIT-ASSET group '{group}' done")
