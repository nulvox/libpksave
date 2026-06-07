# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
ctypes wrapper for libpksave.

Minimal Python surface for the mGBA-Lua test harness:
    load_library(path=None) -> CDLL
    detect(buf: bytes) -> Game
    open(buf: bytes) -> Save
    get_trainer_name(save: Save) -> str
    close(save: Save) -> None

The library decodes the cartridge charset into ASCII inside
pksave_get_trainer (see libpksave/include/pksave.h), so the
trainer name returned here is already a regular Python str.
"""

import ctypes
import enum
import os
import sys
from ctypes import (
    CDLL,
    POINTER,
    Structure,
    c_char,
    c_int,
    c_size_t,
    c_uint8,
    c_uint16,
    c_uint32,
    c_void_p,
)
from typing import Optional


# ── enums (mirror of pksave.h) ────────────────────────────────────────────────

class Game(enum.IntEnum):
    UNKNOWN  = 0
    RB       = 1
    YELLOW   = 2
    GS       = 3
    CRYSTAL  = 4
    RS       = 5
    EMERALD  = 6
    FRLG     = 7
    DP       = 8
    PLATINUM = 9
    HGSS     = 10
    BW       = 11
    BW2      = 12


# pksave error codes
PKSAVE_OK           = 0
PKSAVE_ERR_NULL     = -1
PKSAVE_ERR_RANGE    = -2
PKSAVE_ERR_GAME     = -3
PKSAVE_ERR_CORRUPT  = -4


# ── struct mirror (only what the smoke test consumes) ────────────────────────

class PksaveTrainer(Structure):
    """Mirror of pksave_trainer_t from pksave.h.

    `name` is a 13-byte null-terminated buffer holding ASCII (the library
    has already decoded the cartridge charset).
    """
    _fields_ = [
        ("name",   c_char * 13),
        ("tid",    c_uint16),
        ("sid",    c_uint16),
        ("money",  c_uint32),
        ("badges", c_uint8),
    ]


class PksavePokemon(Structure):
    """Mirror of pksave_pokemon_t from pksave.h.

    Field order and sizing verified against a host gcc sizeof/offsetof
    probe (sizeof = 72 bytes).
    """
    _fields_ = [
        ("species",   c_uint16),
        ("held_item", c_uint16),
        ("level",     c_uint8),
        ("nature",    c_uint8),
        ("ability",   c_uint8),
        ("is_shiny",  c_uint8),
        ("move_ids",  c_uint8 * 4),
        ("move_pp",   c_uint8 * 4),
        ("evs",       c_uint16 * 6),
        ("ivs",       c_uint8 * 6),
        ("nickname",  c_char * 13),
        ("ot_name",   c_char * 13),
        ("stat",      c_uint16 * 6),
    ]


# ── loader ───────────────────────────────────────────────────────────────────

_HERE = os.path.dirname(os.path.abspath(__file__))

# Plausible locations, in priority order.  The default search starts from the
# repo root (two parents up from this file: frontend-gba/tests/automation/).
_REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))

_DEFAULT_SEARCH_PATHS = [
    os.path.join(_REPO_ROOT, "libpksave", "libpksave.so"),
    os.path.join(os.getcwd(), "libpksave", "libpksave.so"),
    os.path.join(os.getcwd(), "libpksave.so"),
    "libpksave.so",
]


def _candidate_paths(explicit: Optional[str]) -> list[str]:
    if explicit:
        return [explicit]
    paths: list[str] = []
    env = os.environ.get("PKSAVE_SO")
    if env:
        paths.append(env)
    ld_lib = os.environ.get("LD_LIBRARY_PATH", "")
    for d in ld_lib.split(os.pathsep):
        if d:
            paths.append(os.path.join(d, "libpksave.so"))
    paths.extend(_DEFAULT_SEARCH_PATHS)
    return paths


def load_library(path: Optional[str] = None) -> CDLL:
    """Load libpksave.so and bind the symbols the wrapper uses.

    If `path` is None, search PKSAVE_SO, then LD_LIBRARY_PATH entries,
    then a small list of repo-relative defaults.
    """
    last_err: Optional[Exception] = None
    for candidate in _candidate_paths(path):
        if not os.path.isfile(candidate):
            continue
        try:
            lib = CDLL(candidate)
        except OSError as exc:
            last_err = exc
            continue
        _bind(lib)
        return lib
    tried = "\n  ".join(_candidate_paths(path))
    raise FileNotFoundError(
        f"libpksave.so not found.  Tried:\n  {tried}\n"
        f"Build it with `make -C libpksave lib` or set PKSAVE_SO.\n"
        f"Last load error: {last_err!r}"
    )


def _bind(lib: CDLL) -> None:
    """Bind argtypes/restypes for the symbols we call from Python."""
    # pksave_game_t pksave_detect(const uint8_t *sram, size_t len);
    lib.pksave_detect.argtypes = [POINTER(c_uint8), c_size_t]
    lib.pksave_detect.restype = c_int

    # pksave_save_t *pksave_open(uint8_t *sram, size_t len, pksave_game_t game);
    lib.pksave_open.argtypes = [POINTER(c_uint8), c_size_t, c_int]
    lib.pksave_open.restype = c_void_p

    # void pksave_close(pksave_save_t *save);
    lib.pksave_close.argtypes = [c_void_p]
    lib.pksave_close.restype = None

    # int pksave_get_trainer(pksave_save_t *save, pksave_trainer_t *out);
    lib.pksave_get_trainer.argtypes = [c_void_p, POINTER(PksaveTrainer)]
    lib.pksave_get_trainer.restype = c_int

    # int pksave_get_party(pksave_save_t *save, pksave_pokemon_t out[6], uint8_t *count);
    lib.pksave_get_party.argtypes = [c_void_p, POINTER(PksavePokemon), POINTER(c_uint8)]
    lib.pksave_get_party.restype = c_int


# ── Pythonic wrappers ────────────────────────────────────────────────────────

class Save:
    """Opaque handle: holds the C `pksave_save_t*` plus a reference to the
    backing SRAM buffer so the GC doesn't free it while libpksave reads it."""

    __slots__ = ("_lib", "_handle", "_sram", "_closed")

    def __init__(self, lib: CDLL, handle: int, sram: ctypes.Array) -> None:
        self._lib = lib
        self._handle = handle
        self._sram = sram   # keep alive
        self._closed = False

    @property
    def handle(self) -> int:
        return self._handle

    def __bool__(self) -> bool:
        return not self._closed and self._handle is not None

    def __del__(self) -> None:
        # Best-effort close on GC.
        try:
            if not self._closed and self._handle:
                self._lib.pksave_close(self._handle)
                self._closed = True
        except Exception:
            pass


def _as_uint8_buf(data: bytes) -> ctypes.Array:
    arr = (c_uint8 * len(data))(*data)
    return arr


def detect(buf: bytes, *, lib: Optional[CDLL] = None) -> Game:
    """Run pksave_detect on a save buffer; returns the Game enum."""
    if lib is None:
        lib = load_library()
    arr = _as_uint8_buf(buf)
    rc = lib.pksave_detect(arr, len(buf))
    return Game(rc)


def open(buf: bytes, game: Optional[Game] = None,
         *, lib: Optional[CDLL] = None) -> Save:  # noqa: A001
    """Open a save buffer.  If `game` is omitted, auto-detect first.

    Returns a Save handle.  The internal SRAM buffer is kept alive by the
    Save object — the original `bytes` is copied into a ctypes array, so
    libpksave writes (during flush) do NOT mutate the caller's bytes.
    """
    if lib is None:
        lib = load_library()
    arr = _as_uint8_buf(buf)
    if game is None:
        game = Game(lib.pksave_detect(arr, len(buf)))
    if game == Game.UNKNOWN:
        raise ValueError("pksave_detect could not identify save format")
    handle = lib.pksave_open(arr, len(buf), int(game))
    if not handle:
        raise RuntimeError(f"pksave_open failed for game={game.name}")
    return Save(lib, handle, arr)


def get_trainer_name(save: Save) -> str:
    """Read the trainer name.  Returned as a Python str — libpksave has
    already decoded the cartridge charset to ASCII."""
    if not save:
        raise ValueError("save is closed or invalid")
    tr = PksaveTrainer()
    rc = save._lib.pksave_get_trainer(save._handle, ctypes.byref(tr))
    if rc != PKSAVE_OK:
        raise RuntimeError(f"pksave_get_trainer rc={rc}")
    # tr.name is a 13-byte buffer, null-terminated.  ctypes c_char arrays
    # auto-trim at the first NUL when accessed as .value.
    return tr.name.decode("ascii", errors="replace")


def get_trainer(save: Save) -> PksaveTrainer:
    """Return the full trainer struct (for callers that want TID/money/badges)."""
    if not save:
        raise ValueError("save is closed or invalid")
    tr = PksaveTrainer()
    rc = save._lib.pksave_get_trainer(save._handle, ctypes.byref(tr))
    if rc != PKSAVE_OK:
        raise RuntimeError(f"pksave_get_trainer rc={rc}")
    return tr


def get_party(save: Save) -> tuple[list[PksavePokemon], int]:
    """Return (party_array, count).  party_array always has 6 entries; only
    the first `count` are meaningful (filled slots).  Each entry is a
    PksavePokemon struct with the standard fields decoded for the game
    variant the save was opened as."""
    if not save:
        raise ValueError("save is closed or invalid")
    arr = (PksavePokemon * 6)()
    count = c_uint8(0)
    rc = save._lib.pksave_get_party(save._handle, arr, ctypes.byref(count))
    if rc != PKSAVE_OK:
        raise RuntimeError(f"pksave_get_party rc={rc}")
    return list(arr), int(count.value)


def close(save: Save) -> None:
    """Close a Save handle.  Idempotent."""
    if save and not save._closed:
        save._lib.pksave_close(save._handle)
        save._closed = True


__all__ = [
    "Game",
    "PksaveTrainer",
    "PksavePokemon",
    "Save",
    "load_library",
    "detect",
    "open",
    "get_trainer_name",
    "get_trainer",
    "get_party",
    "close",
    "PKSAVE_OK",
    "PKSAVE_ERR_NULL",
    "PKSAVE_ERR_RANGE",
    "PKSAVE_ERR_GAME",
    "PKSAVE_ERR_CORRUPT",
]
