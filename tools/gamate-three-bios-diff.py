#!/usr/bin/env python3
# Copyright (c) 2026 Ry
# SPDX-License-Identifier: GPL-2.0-or-later

"""Launch UMC, Bit, and SWK RetroArch instances with identical differential inputs."""

import argparse
import curses
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass


RELEASE_BIOS_NAMES = ("gamate_bios_umc.bin", "gamate_bios_bit.bin", "gamate_bios_swk.bin")
GAME_KEYS = {
    curses.KEY_UP: "Up",
    curses.KEY_DOWN: "Down",
    curses.KEY_LEFT: "Left",
    curses.KEY_RIGHT: "Right",
    10: "Return",
    13: "Return",
    27: "Escape",
    ord("z"): "z",
    ord("Z"): "z",
    ord("x"): "x",
    ord("X"): "x",
    ord("a"): "a",
    ord("A"): "a",
    ord("s"): "s",
    ord("S"): "s",
    ord("q"): "q",
    ord("Q"): "q",
    ord("w"): "w",
    ord("W"): "w",
    ord("r"): "r",
    ord("R"): "r",
}


@dataclass
class Instance:
    name: str
    bios_priority: str
    bios_source: Path
    port: int
    config: Path
    system: Path
    state: Path
    saves: Path
    screenshots: Path
    replay: Path
    process: subprocess.Popen | None = None
    window_ids: list[int] | None = None


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def find_bios(directory: Path, name: str, explicit: Path | None = None) -> Path:
    candidate = explicit if explicit is not None else directory / name
    if not candidate.is_file():
        raise SystemExit(f"missing BIOS or ROM input: {candidate}")
    if candidate.stat().st_size != 4096:
        raise SystemExit(f"invalid BIOS size (expected 4096): {candidate}")
    return candidate.resolve()


def write_config(instance: Instance, core: Path, rom: Path, args: argparse.Namespace) -> None:
    instance.config.write_text(
        "\n".join([
            'video_fullscreen = "false"',
            'video_windowed_fullscreen = "false"',
            f'video_windowed_position_width = "{args.width}"',
            f'video_windowed_position_height = "{args.height}"',
            f'video_windowed_position_x = "{args.x + instance_number(instance.name) * args.width // 2}"',
            f'video_windowed_position_y = "{args.y}"',
            'video_window_save_positions = "false"',
            'pause_nonactive = "false"',
            'suspend_screensaver_enable = "false"',
            'network_cmd_enable = "true"',
            f'network_cmd_port = "{instance.port}"',
            f'system_directory = "{instance.system}"',
            f'savefile_directory = "{instance.saves}"',
            f'savestate_directory = "{instance.state}"',
            f'screenshot_directory = "{instance.screenshots}"',
            'input_max_users = "1"',
            'input_remapping_directory = "/tmp"',
            'config_save_on_exit = "false"',
            'menu_driver = "rgui"',
            f'superwhizkid_bios = "{instance.bios_priority}"',
            'superwhizkid_skip_intro = "off"',
            'superwhizkid_tap_latch = "off"',
            'superwhizkid_audio_output = "stereo"',
            'superwhizkid_lcd_ghosting = "off"',
            'superwhizkid_flicker_reduction = "off"',
            "",
        ]),
        encoding="utf-8",
    )


def instance_number(name: str) -> int:
    return {"umc": 0, "bit": 1, "swk": 2}[name]


def udp_command(instance: Instance, command: str) -> bool:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
            udp.settimeout(0.25)
            udp.sendto((command + "\n").encode("ascii"), ("127.0.0.1", instance.port))
        return True
    except OSError:
        return False


def find_windows(instance: Instance) -> list[int]:
    if shutil.which("xdotool") is None:
        return []
    if instance.process is None:
        return []
    result = subprocess.run(
        ["xdotool", "search", "--pid", str(instance.process.pid)],
        capture_output=True,
        text=True,
        timeout=3,
        check=False,
    )
    return [int(value) for value in result.stdout.split() if value.isdigit()]


def wait_for_windows(instances: list[Instance], timeout: float = 20.0) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        all_found = True
        for instance in instances:
            if instance.window_ids is None:
                instance.window_ids = []
            instance.window_ids = find_windows(instance)
            all_found &= bool(instance.window_ids)
        if all_found:
            return True
        time.sleep(0.25)
    return False


def broadcast_key(instances: list[Instance], key_name: str) -> None:
    if shutil.which("xdotool") is None:
        return
    for instance in instances:
        for window_id in instance.window_ids or []:
            subprocess.run(
                ["xdotool", "key", "--window", str(window_id), key_name],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=2,
                check=False,
            )


def broadcast_command(instances: list[Instance], command: str) -> None:
    for instance in instances:
        udp_command(instance, command)


def recent_files(directory: Path, suffix: str, before: set[Path]) -> list[Path]:
    return sorted(
        (path for path in directory.glob("*" + suffix) if path not in before),
        key=lambda path: path.stat().st_mtime_ns,
    )


def pixel_hash(path: Path) -> str | None:
    try:
        from PIL import Image
    except ImportError:
        return sha256(path)
    try:
        with Image.open(path) as image:
            return hashlib.sha256(image.convert("RGBA").tobytes()).hexdigest()
    except OSError:
        return sha256(path)


def snapshot(instances: list[Instance], session: Path, serial: int) -> None:
    before_states = {instance.name: set(instance.state.glob("*")) for instance in instances}
    before_images = {instance.name: set(instance.screenshots.glob("*")) for instance in instances}

    broadcast_command(instances, "PAUSE_TOGGLE")
    time.sleep(0.25)
    broadcast_command(instances, "SAVE_STATE")
    broadcast_command(instances, "SCREENSHOT")
    time.sleep(1.0)

    report_path = session / "snapshots.tsv"
    header = ("snapshot\tbios\tstate_size\tstate_sha256\tscreenshot_size\tscreenshot_sha256\t"
              "state_equal_previous\tstate_equal_umc_bit\tstate_equal_umc_swk\t"
              "screenshot_equal_umc_bit\tscreenshot_equal_umc_swk\n")
    if not report_path.exists():
        report_path.write_text(header, encoding="utf-8")

    state_by_name = {}
    image_by_name = {}
    for instance in instances:
        state_files = recent_files(instance.state, ".state", before_states[instance.name])
        state_files.extend(
            path for path in before_states[instance.name]
            if path.suffix == ".state" and path not in state_files
        )
        state_files = sorted(set(state_files), key=lambda path: path.stat().st_mtime_ns)
        state_path = state_files[-1] if state_files else None

        image_files = recent_files(instance.screenshots, ".png", before_images[instance.name])
        image_files = sorted(set(image_files), key=lambda path: path.stat().st_mtime_ns)
        image_path = image_files[-1] if image_files else None

        state_size = state_path.stat().st_size if state_path else 0
        state_hash = sha256(state_path) if state_path else "-"
        image_size = image_path.stat().st_size if image_path else 0
        image_hash = pixel_hash(image_path) if image_path else "-"
        state_by_name[instance.name] = state_hash
        image_by_name[instance.name] = image_hash
        print(f"[{instance.name}] state={state_hash} screenshot={image_hash}")

    previous_states = {}
    if report_path.exists():
        import csv
        with report_path.open("r", newline="", encoding="utf-8") as report:
            for row in csv.DictReader(report, delimiter="\t"):
                if int(row["snapshot"]) == serial - 1:
                    previous_states[row["bios"]] = row["state_sha256"]

    rows = []
    for instance in instances:
        state_hash = state_by_name[instance.name]
        image_hash = image_by_name[instance.name]
        state_files = list(instance.state.glob("*"))
        image_files = list(instance.screenshots.glob("*"))
        state_path = max(
            (path for path in state_files if path.suffix == ".state"),
            key=lambda path: path.stat().st_mtime_ns,
            default=None,
        )
        image_path = max(
            (path for path in image_files if path.suffix == ".png"),
            key=lambda path: path.stat().st_mtime_ns,
            default=None,
        )
        state_size = state_path.stat().st_size if state_path else 0
        image_size = image_path.stat().st_size if image_path else 0
        equal_previous = (
            "true" if previous_states.get(instance.name) == state_hash
            else "false"
        )
        rows.append(
            f"{serial}\t{instance.name}\t{state_size}\t{state_hash}\t"
            f"{image_size}\t{image_hash}\t{equal_previous}\t"
            f"{'true' if state_by_name['umc'] == state_by_name['bit'] else 'false'}\t"
            f"{'true' if state_by_name['umc'] == state_by_name['swk'] else 'false'}\t"
            f"{'true' if image_by_name['umc'] == image_by_name['bit'] else 'false'}\t"
            f"{'true' if image_by_name['umc'] == image_by_name['swk'] else 'false'}"
        )

    with report_path.open("a", encoding="utf-8") as report:
        report.write("\n".join(rows) + "\n")
    if (image_by_name["umc"] != image_by_name["bit"] or
            image_by_name["umc"] != image_by_name["swk"] or
            state_by_name["umc"] != state_by_name["bit"] or
            state_by_name["umc"] != state_by_name["swk"]):
        print(f"divergence detected at snapshot {serial}")
    broadcast_command(instances, "PAUSE_TOGGLE")


def curses_ui(instances: list[Instance], session: Path) -> None:
    snapshot_serial = 0

    def run(stdscr) -> None:
        nonlocal snapshot_serial
        curses.curs_set(0)
        stdscr.nodelay(True)
        stdscr.keypad(True)

        while True:
            stdscr.erase()
            stdscr.addstr(0, 0, "Three-BIOS interactive differential tracer")
            stdscr.addstr(1, 0, f"session: {session}")
            stdscr.addstr(2, 0, "game keys: arrows, Enter/Esc, Z X A S Q W")
            stdscr.addstr(3, 0, "commands: p pause  v frameadvance  s snapshot  t screenshot")
            stdscr.addstr(4, 0, "          r reset  u resume  q quit  ? help")
            stdscr.refresh()

            key = stdscr.getch()
            if key == -1:
                time.sleep(0.01)
                continue
            if key == ord("Q"):
                broadcast_command(instances, "QUIT")
                return
            if key == ord("?"):
                broadcast_command(instances, "MENU_TOGGLE")
                continue
            if key == ord("P"):
                broadcast_command(instances, "PAUSE_TOGGLE")
                continue
            if key == ord("U"):
                broadcast_command(instances, "PAUSE_TOGGLE")
                continue
            if key == ord("V"):
                broadcast_command(instances, "PAUSE_TOGGLE")
                broadcast_command(instances, "FRAMEADVANCE")
                continue
            if key == ord("R"):
                broadcast_command(instances, "RESET")
                continue
            if key == ord("S"):
                snapshot_serial += 1
                snapshot(instances, session, snapshot_serial)
                continue
            if key == ord("T"):
                broadcast_command(instances, "SCREENSHOT")
                continue
            if key in (curses.KEY_UP, curses.KEY_DOWN, curses.KEY_LEFT, curses.KEY_RIGHT,
                       10, 13, 27) or chr(key).lower() in ("z", "x", "a", "s", "q", "w"):
                broadcast_key(instances, GAME_KEYS[key])

    curses.wrapper(run)


def replay_ui(instances: list[Instance], replay: Path) -> None:
    for instance in instances:
        instance.replay = replay
    print(f"Replaying {replay}")
    print("No interactive key broadcast is active.")
    while any(instance.process and instance.process.poll() is None for instance in instances):
        time.sleep(0.25)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run UMC, Bit, and SWK RetroArch instances with identical inputs.",
    )
    parser.add_argument("rom")
    parser.add_argument("--core", default="build/superwhizkid_libretro.so")
    parser.add_argument("--bios-dir")
    parser.add_argument("--umc-bios")
    parser.add_argument("--bit-bios")
    parser.add_argument("--swk-bios")
    parser.add_argument("--retroarch", default="retroarch")
    parser.add_argument("--session")
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--x", type=int, default=0)
    parser.add_argument("--y", type=int, default=0)
    parser.add_argument("--replay")
    parser.add_argument("--record-replay", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    rom = Path(args.rom).resolve()
    core = Path(args.core).resolve()
    if not rom.is_file():
        raise SystemExit(f"missing ROM: {rom}")
    if not core.is_file():
        raise SystemExit(f"missing core: {core}")
    if args.replay and args.record_replay:
        raise SystemExit("--replay and --record-replay are mutually exclusive")

    bios_dir = Path(args.bios_dir).resolve() if args.bios_dir else rom.parent
    umc = find_bios(bios_dir, "gamate_bios_umc.bin", Path(args.umc_bios) if args.umc_bios else None)
    bit = find_bios(bios_dir, "gamate_bios_bit.bin", Path(args.bit_bios) if args.bit_bios else None)
    swk_candidate = Path(args.swk_bios) if args.swk_bios else Path("build-bios/gamate_bios_swk.bin")
    swk = find_bios(bios_dir, "gamate_bios_swk.bin", swk_candidate)

    session = Path(args.session).resolve() if args.session else Path(
        tempfile.mkdtemp(prefix="superwhizkid-three-bios-", dir="/tmp")
    )
    session.mkdir(parents=True, exist_ok=True)

    replay = Path(args.replay).resolve() if args.replay else None
    if replay is not None and not replay.is_file():
        raise SystemExit(f"missing replay: {replay}")

    instances = []
    for name, priority, bios in (
        ("umc", "umc", umc),
        ("bit", "bit", bit),
        ("swk", "swk", swk),
    ):
        root = session / name
        system = root / "system"
        state = root / "states"
        saves = root / "saves"
        screenshots = root / "screenshots"
        replay_dir = root / "replays"
        for directory in (system, state, saves, screenshots, replay_dir):
            directory.mkdir(parents=True, exist_ok=True)
        system_bios = system / bios.name
        if system_bios.exists() or system_bios.is_symlink():
            system_bios.unlink()
        system_bios.symlink_to(bios)
        instance = Instance(
            name=name,
            bios_priority=priority,
            bios_source=bios,
            port=55355 + instance_number(name),
            config=root / "retroarch.cfg",
            system=system,
            state=state,
            saves=saves,
            screenshots=screenshots,
            replay=replay_dir / f"{name}.bsv",
        )
        write_config(instance, core, rom, args)
        instances.append(instance)

    manifest = {
        "rom_sha256": sha256(rom),
        "core_sha256": sha256(core),
        "bios_sha256": {instance.name: sha256(instance.bios_source) for instance in instances},
        "bios_names": {instance.name: instance.bios_source.name for instance in instances},
        "instances": {
            instance.name: {
                "bios_priority": instance.bios_priority,
                "config": str(instance.config),
                "system": str(instance.system),
                "state": str(instance.state),
                "screenshot": str(instance.screenshots),
                "port": instance.port,
            }
            for instance in instances
        },
    }
    (session / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    if args.dry_run:
        print(json.dumps(manifest, indent=2, sort_keys=True))
        return 0
    if shutil.which(args.retroarch) is None:
        raise SystemExit(f"missing RetroArch executable: {args.retroarch}")
    if replay is None and shutil.which("xdotool") is None:
        raise SystemExit(
            "interactive broadcast requires xdotool; install it or use --replay existing.bsv"
        )

    for instance in instances:
        command = [
            args.retroarch,
            "-v",
            "-c",
            str(Path.home() / ".config" / "retroarch" / "retroarch.cfg"),
            "--appendconfig",
            str(instance.config),
            "-L",
            str(core),
        ]
        if replay is not None:
            command.extend(["-P", str(replay), "--eof-exit"])
        elif args.record_replay:
            command.extend(["-R", str(instance.replay)])
        command.append(str(rom))
        instance.process = subprocess.Popen(
            command,
            stdout=(session / f"{instance.name}.stdout.log").open("wb"),
            stderr=(session / f"{instance.name}.stderr.log").open("wb"),
            start_new_session=True,
        )

    if replay is not None:
        replay_ui(instances, replay)
        return 0

    if not wait_for_windows(instances):
        print("warning: could not discover all RetroArch X11 windows", file=sys.stderr)
    if args.record_replay:
        print(f"Recording replay inputs in {session}")
    curses_ui(instances, session)
    for instance in instances:
        if instance.process is not None and instance.process.poll() is None:
            instance.process.terminate()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
