"""
Realtime Row Protocol Pygame Builder - live split-screen version

What this version is for:
- DEBUG keeps the old fixed-column instruction/menu/settings behavior.
- SNAKE, DRAW, and BATTLESHIP are realtime split screens:
    left  = paint/edit panel
    right = scrolling instruction panel with the same scroll dimensions/settings
- Press START in Snake/Draw to scroll the calibration row first: X99Y99.
- After START, every new paint action immediately queues rows into the right scroll panel.
- If the previous state row is already the same, the state row is not repeated.
    Example: WALL, X01Y01, X02Y01, X03Y01
    instead of WALL, X01Y01, WALL, X02Y01, WALL, X03Y01
- Snake has a CLEAR ALL button that sends CLEAR and clears wall/apple/portal cells.
- Battleship is an added game/designer, not a replacement for debug/snake/draw/settings.
- Debug hardware reminder:
    KEY0 in VGA debug = request image capture through IMG/Arduino.
    KEY1 in VGA debug = use current debug image as Draw Pixel background.

Install/run:
    pip install pygame
    python python_realtime_live_split_updated.txt

Realtime 6-character protocol:
    X99Y99  calibration row
    WALL    snake wall state
    APPLE   snake apple state
    PORTA   snake portal A state
    PORTB   snake portal B state
    ERASE   erase one snake/draw cell
    CLEAR   clear all snake wall/apple/portal state on FPGA side
    RED     draw red state
    GREEN   draw green state
    BLUE    draw blue state
    YELLOW  draw yellow state
    BLACK   draw black state
    WHITE   draw white state
    SHIP    battleship ship-cell state
    DONE    battleship layout marker
    X12Y09  apply current state/color at coordinate x=12, y=9
"""

import sys
import random
from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Tuple

import pygame

# ============================================================
# Window / FPS
# ============================================================

MENU_WIDTH = 1100
MENU_HEIGHT = 740
FPS = 60

SCROLL_WIDTH = 800
SCROLL_HEIGHT = 844

REALTIME_CHARS_PER_LINE = 6
REALTIME_CALIB_ROW = "X99Y99"

DEBUG_CHARS_PER_LINE = 8
DEBUG_CALIB_PREFIX = "START"
NEWLINES_BETWEEN_INSTRUCTIONS = 2
CENTER_TEXT = True
LEFT_RIGHT_PADDING = 120

# Per-menu scroll speeds.
# Debug keeps the old slower speed; coordinate games default faster.
DEFAULT_DEBUG_SPEED_PPS = 250
DEFAULT_COORD_GAME_SPEED_PPS = 400
DEFAULT_DRAW_SPEED_PPS = 400

SNAKE_COLS = 32
SNAKE_ROWS = 22
DRAW_COLS = 96
DRAW_ROWS = 96
BATTLE_COLS = 10
BATTLE_ROWS = 10
BATTLE_FLEET_SIZES = [5, 4, 3, 3, 2]

# ============================================================
# Colors
# ============================================================

BLACK = (8, 8, 14)
DARK = (18, 18, 32)
PANEL = (28, 28, 48)
PANEL_2 = (38, 38, 64)
WHITE = (238, 238, 238)
GRAY = (150, 150, 160)
DIM = (90, 90, 105)
GREEN = (70, 255, 130)
CYAN = (70, 230, 255)
YELLOW = (255, 220, 70)
RED = (255, 80, 80)
PURPLE = (220, 90, 255)
BLUE = (70, 120, 255)
ORANGE = (255, 160, 60)

DRAW_COLOR_RGB: Dict[str, Tuple[int, int, int]] = {
    "RED": RED,
    "GREEN": GREEN,
    "BLUE": BLUE,
    "YELLOW": YELLOW,
    "BLACK": (20, 20, 28),
    "WHITE": WHITE,
}

SNAKE_TOOL_RGB: Dict[str, Tuple[int, int, int]] = {
    "WALL": GRAY,
    "APPLE": RED,
    "PORTA": PURPLE,
    "PORTB": BLUE,
    "ERASE": DARK,
}

BATTLE_TOOL_RGB: Dict[str, Tuple[int, int, int]] = {
    "SHIP": (185, 205, 230),
    "ERASE": DARK,
}

# ============================================================
# Protocol helpers
# ============================================================


def normalize_instruction(text: str) -> str:
    return " ".join(text.upper().split())


def compact_instruction(text: str) -> str:
    return "".join(normalize_instruction(text).split())


def build_start_calibration_line(max_chars: int, prefix: str = DEBUG_CALIB_PREFIX) -> str:
    prefix = prefix.upper()
    if max_chars <= 0:
        return ""
    if len(prefix) >= max_chars:
        return prefix[:max_chars]
    remaining = max_chars - len(prefix)
    suffix = "".join(str(i % 10) for i in range(remaining, 0, -1))
    return prefix + suffix


def pad_row(row: str, width: int) -> str:
    return row.upper()[:width].ljust(width)


def coord_row(x: int, y: int) -> str:
    x = max(0, min(99, int(x)))
    y = max(0, min(99, int(y)))
    return f"X{x:02d}Y{y:02d}"


def is_realtime_coord_row(row: str) -> bool:
    row = row.strip().upper()
    return (
        len(row) >= 6 and
        row[0] == "X" and
        row[1:3].isdigit() and
        row[3] in ("Y", "T") and
        row[4:6].isdigit()
    )


def is_realtime_state_row(row: str) -> bool:
    row = row.strip().upper()
    return row != "" and not is_realtime_coord_row(row)


def rows_to_text(rows: List[str], width: int) -> str:
    return "\n".join(pad_row(row, width) for row in rows)


def fixed_columnize_text(raw_text: str, settings) -> str:
    """Old debug path: START321 style calibration + configurable fixed rows."""
    max_chars = max(1, int(settings.max_chars_per_line))
    out: List[str] = []

    if settings.enable_start_calibration_line:
        out.append(build_start_calibration_line(max_chars, settings.start_calibration_prefix))

    for raw_line in raw_text.upper().split("\n"):
        line = raw_line.rstrip("\r")
        if line.strip() == "":
            out.append(" " * max_chars)
            continue

        line = normalize_instruction(line)
        while len(line) > max_chars:
            out.append(line[:max_chars])
            line = line[max_chars:]
        out.append(line.ljust(max_chars))

    return "\n".join(out)


def realtime_columnize(rows: List[str]) -> str:
    return rows_to_text([REALTIME_CALIB_ROW] + rows, REALTIME_CHARS_PER_LINE)


def compress_realtime_paint_rows(cells: Dict[Tuple[int, int], str], priority: List[str]) -> List[str]:
    rows: List[str] = []
    for token in priority:
        coords = sorted([pos for pos, value in cells.items() if value == token], key=lambda p: (p[1], p[0]))
        if not coords:
            continue
        rows.append(token)
        for x, y in coords:
            rows.append(coord_row(x, y))
    return rows

# ============================================================
# App state
# ============================================================


@dataclass
class ScrollSettings:
    phone_width: int = SCROLL_WIDTH
    phone_height: int = SCROLL_HEIGHT
    bg_color_name: str = "white"
    random_color: bool = False
    mode: str = "vertical"
    direction: str = "bottom_to_top"
    frequency: int = 90
    speed_pps: int = DEFAULT_DEBUG_SPEED_PPS  # legacy/debug fallback
    debug_speed_pps: int = DEFAULT_DEBUG_SPEED_PPS
    snake_speed_pps: int = DEFAULT_COORD_GAME_SPEED_PPS
    draw_speed_pps: int = DEFAULT_DRAW_SPEED_PPS
    battle_speed_pps: int = DEFAULT_COORD_GAME_SPEED_PPS
    font_family: str = "consolas"
    font_size: int = 70
    font_weight_bold: bool = True
    char_gap: int = 20
    line_gap: int = 75
    margin: int = 50
    center_text: bool = CENTER_TEXT
    left_right_padding: int = LEFT_RIGHT_PADDING
    newlines_between_instructions: int = NEWLINES_BETWEEN_INSTRUCTIONS
    max_chars_per_line: int = DEBUG_CHARS_PER_LINE
    enable_start_calibration_line: bool = True
    start_calibration_prefix: str = DEBUG_CALIB_PREFIX


@dataclass
class AppState:
    screen: str = "main"
    previous_screen: str = "main"
    protocol: str = "debug"
    settings: ScrollSettings = field(default_factory=ScrollSettings)
    scroll_text: str = ""
    raw_rows: List[str] = field(default_factory=list)
    raw_instruction_text: str = ""


def speed_for_protocol(settings: ScrollSettings, protocol: str) -> int:
    """Return scroll speed for the current menu/protocol."""
    p = (protocol or "debug").lower()
    if p == "snake":
        return int(settings.snake_speed_pps)
    if p == "draw":
        return int(settings.draw_speed_pps)
    if p in ("battle", "battleship"):
        return int(settings.battle_speed_pps)
    if p == "debug":
        return int(settings.debug_speed_pps)
    return int(getattr(settings, "speed_pps", DEFAULT_DEBUG_SPEED_PPS))

# ============================================================
# Basic widgets
# ============================================================


class Button:
    def __init__(self, rect, text: str, callback: Callable[[], None], bg=PANEL_2, fg=WHITE):
        self.rect = pygame.Rect(rect)
        self.text = text
        self.callback = callback
        self.bg = bg
        self.fg = fg
        self.hover = False

    def handle_event(self, event):
        if event.type == pygame.MOUSEMOTION:
            self.hover = self.rect.collidepoint(event.pos)
        elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 1 and self.rect.collidepoint(event.pos):
            self.callback()

    def draw(self, surface, font):
        color = tuple(min(255, c + 26) for c in self.bg) if self.hover else self.bg
        pygame.draw.rect(surface, color, self.rect, border_radius=8)
        pygame.draw.rect(surface, CYAN if self.hover else GRAY, self.rect, 2, border_radius=8)
        label = font.render(self.text, True, self.fg)
        surface.blit(label, label.get_rect(center=self.rect.center))


class TextInput:
    def __init__(self, rect, text="", placeholder="", max_len=120):
        self.rect = pygame.Rect(rect)
        self.text = text
        self.placeholder = placeholder
        self.max_len = max_len
        self.active = False

    def handle_event(self, event):
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            self.active = self.rect.collidepoint(event.pos)
        elif event.type == pygame.KEYDOWN and self.active:
            if event.key == pygame.K_BACKSPACE:
                self.text = self.text[:-1]
            elif event.key in (pygame.K_RETURN, pygame.K_TAB):
                self.active = False
            elif event.unicode and len(self.text) < self.max_len and 32 <= ord(event.unicode) <= 126:
                self.text += event.unicode

    def draw(self, surface, font):
        pygame.draw.rect(surface, (14, 14, 24), self.rect, border_radius=6)
        pygame.draw.rect(surface, CYAN if self.active else GRAY, self.rect, 2, border_radius=6)
        shown = self.text if self.text else self.placeholder
        color = WHITE if self.text else DIM
        rendered = font.render(shown, True, color)
        surface.blit(rendered, (self.rect.x + 10, self.rect.y + 8))


class Toggle:
    def __init__(self, rect, label: str, value=False):
        self.rect = pygame.Rect(rect)
        self.label = label
        self.value = value

    def handle_event(self, event):
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1 and self.rect.collidepoint(event.pos):
            self.value = not self.value

    def draw(self, surface, font):
        box = pygame.Rect(self.rect.x, self.rect.y, 24, 24)
        pygame.draw.rect(surface, GREEN if self.value else DARK, box, border_radius=4)
        pygame.draw.rect(surface, CYAN, box, 2, border_radius=4)
        label = font.render(self.label, True, WHITE)
        surface.blit(label, (self.rect.x + 34, self.rect.y - 1))

# ============================================================
# Base screen
# ============================================================


class BaseScreen:
    def __init__(self, app):
        self.app = app
        self.buttons: List[Button] = []

    def handle_event(self, event):
        for button in self.buttons:
            button.handle_event(event)

    def update(self, dt: float):
        pass

    def draw(self, surface):
        raise NotImplementedError

# ============================================================
# Main menu
# ============================================================


class MainMenuScreen(BaseScreen):
    def __init__(self, app):
        super().__init__(app)
        self.title_font = pygame.font.SysFont("consolas", 38, bold=True)
        self.font = pygame.font.SysFont("consolas", 22, bold=True)
        self.small = pygame.font.SysFont("consolas", 16)
        self.buttons = [
            Button((365, 190, 370, 48), "DEBUG MENU: INSTRUCTION", lambda: app.switch("debug")),
            Button((365, 252, 370, 48), "BATTLESHIP DESIGNER", lambda: app.switch("battle")),
            Button((365, 314, 370, 48), "SNAKE LIVE REALTIME", lambda: app.switch("snake")),
            Button((365, 376, 370, 48), "DRAW PIXEL LIVE", lambda: app.switch("draw")),
            Button((365, 438, 370, 48), "SCROLL LAST TEXT", lambda: app.switch("scroll")),
            Button((365, 500, 370, 48), "SCROLL SETTINGS", lambda: app.switch("settings")),
        ]

    def draw(self, surface):
        surface.fill(BLACK)
        draw_retro_frame(surface)
        title = self.title_font.render("PYTHON -> GROVE -> ESP -> FPGA", True, YELLOW)
        surface.blit(title, title.get_rect(center=(MENU_WIDTH // 2, 100)))
        sub = self.font.render("Debug old path. Battle/Snake/Draw live split-screen realtime rows.", True, CYAN)
        surface.blit(sub, sub.get_rect(center=(MENU_WIDTH // 2, 150)))
        for button in self.buttons:
            button.draw(surface, self.font)

        lines = [
            "Live mode: LEFT = draw panel, RIGHT = scroll panel",
            "Press START first: right panel scrolls X99Y99 calibration",
            "After START: each new cell queues one realtime row immediately",
            "Snake clear-all instruction row: CLEAR",
            "Speed defaults: Debug 250, Draw 300, Snake/Battle 400 pps",
            "Debug VGA hardware: KEY0 capture image, KEY1 use image as Draw background",
        ]
        y = 585
        for line in lines:
            txt = self.small.render(line, True, GRAY)
            surface.blit(txt, txt.get_rect(center=(MENU_WIDTH // 2, y)))
            y += 22

# ============================================================
# Debug instruction builder - old path retained
# ============================================================


class DebugScreen(BaseScreen):
    def __init__(self, app):
        super().__init__(app)
        self.big = pygame.font.SysFont("consolas", 28, bold=True)
        self.font = pygame.font.SysFont("consolas", 18)
        self.small = pygame.font.SysFont("consolas", 15)
        self.peripheral = "hex"
        self.instructions: List[str] = []

        self.hex_msg = TextInput((440, 170, 370, 38), "ECE3073", "message")
        self.ledmodule_blink_sec = TextInput((440, 320, 90, 36), "5", "sec", max_len=5)
        self.led_count = TextInput((440, 225, 90, 36), "5", "1-10", max_len=2)
        self.led_blink_sec = TextInput((440, 365, 90, 36), "5", "sec", max_len=5)
        self.speaker_freq = TextInput((440, 225, 150, 36), "50000", "Hz", max_len=8)
        self.speaker_duration = TextInput((440, 320, 90, 36), "1", "sec", max_len=5)

        self.use_red = Toggle((440, 205, 130, 28), "Red", True)
        self.use_green = Toggle((585, 205, 150, 28), "Green", False)
        self.use_yellow = Toggle((745, 205, 150, 28), "Yellow", False)
        self.module_blinking = Toggle((440, 270, 210, 28), "Blinking", True)
        self.led_from_left = Toggle((560, 230, 250, 28), "Direction from left", True)
        self.led_blinking = Toggle((440, 315, 210, 28), "Blinking", True)
        self.speaker_has_duration = Toggle((440, 270, 220, 28), "Add duration", False)

        self.buttons = [
            Button((45, 145, 230, 44), "HEX", lambda: self.set_peripheral("hex")),
            Button((45, 200, 230, 44), "LED MODULE", lambda: self.set_peripheral("ledmodule")),
            Button((45, 255, 230, 44), "FPGA LED", lambda: self.set_peripheral("led")),
            Button((45, 310, 230, 44), "SPEAKER", lambda: self.set_peripheral("speaker")),
            Button((45, 475, 230, 46), "ADD INSTRUCTION", self.add_instruction, GREEN, BLACK),
            Button((45, 535, 230, 46), "DONE / SCROLL", self.done, YELLOW, BLACK),
            Button((45, 595, 230, 40), "CLEAR LIST", self.clear_list, RED, BLACK),
            Button((45, 645, 230, 34), "BACK", lambda: self.app.switch("main")),
        ]
        self.all_inputs = [
            self.hex_msg,
            self.ledmodule_blink_sec,
            self.led_count,
            self.led_blink_sec,
            self.speaker_freq,
            self.speaker_duration,
        ]
        self.all_toggles = [
            self.use_red,
            self.use_green,
            self.use_yellow,
            self.module_blinking,
            self.led_from_left,
            self.led_blinking,
            self.speaker_has_duration,
        ]

    def set_peripheral(self, p: str):
        self.peripheral = p

    def clear_list(self):
        self.instructions.clear()

    def as_int(self, text: str, default: int, min_value=1, max_value=None) -> int:
        try:
            value = int(text.strip())
        except ValueError:
            value = default
        value = max(min_value, value)
        if max_value is not None:
            value = min(max_value, value)
        return value

    def add_instruction(self):
        if self.peripheral == "hex":
            message = self.hex_msg.text.strip() or "ECE3073"
            instruction = f"DISPLAY HEX {message}"
        elif self.peripheral == "ledmodule":
            colors = []
            if self.use_red.value:
                colors.append("red")
            if self.use_green.value:
                colors.append("green")
            if self.use_yellow.value:
                colors.append("yellow")
            if not colors:
                colors.append("red")
            if len(colors) == 1:
                color_text = colors[0]
            elif len(colors) == 2:
                color_text = f"{colors[0]} and {colors[1]}"
            else:
                color_text = f"{colors[0]}, {colors[1]} and {colors[2]}"
            if self.module_blinking.value:
                seconds = self.as_int(self.ledmodule_blink_sec.text, 5)
                instruction = f"{color_text} LED module blinking every {seconds} second"
            else:
                instruction = f"{color_text} LED module static"
        elif self.peripheral == "led":
            count = self.as_int(self.led_count.text, 5, 1, 10)
            direction = "left" if self.led_from_left.value else "right"
            if self.led_blinking.value:
                seconds = self.as_int(self.led_blink_sec.text, 5)
                instruction = f"turn on {count} LED from the {direction} blinking every {seconds} second"
            else:
                instruction = f"turn on {count} LED from the {direction}"
        else:
            freq = self.as_int(self.speaker_freq.text, 50000)
            if self.speaker_has_duration.value:
                seconds = self.as_int(self.speaker_duration.text, 1)
                instruction = f"Set speaker frequency to {freq} Hz for {seconds} second"
            else:
                instruction = f"Set speaker frequency to {freq} Hz"

        if self.peripheral == "hex":
            self.instructions.append(normalize_instruction(instruction))
        else:
            self.instructions.append(compact_instruction(instruction))

    def done(self):
        if not self.instructions:
            self.instructions.append("NO INSTRUCTION ENTERED")
        spacing = "\n" * self.app.state.settings.newlines_between_instructions
        self.app.set_debug_text(spacing.join(self.instructions))
        self.app.switch("scroll")

    def handle_event(self, event):
        super().handle_event(event)
        for widget in self.all_inputs + self.all_toggles:
            widget.handle_event(event)

    def draw_selector_buttons(self, surface):
        for b in self.buttons:
            if b.text.lower().replace(" ", "").startswith(self.peripheral):
                b.bg = BLUE
            elif b.text in ["HEX", "LED MODULE", "FPGA LED", "SPEAKER"]:
                b.bg = PANEL_2
            b.draw(surface, self.font)

    def draw_current_panel(self, surface):
        panel = pygame.Rect(315, 125, 735, 275)
        pygame.draw.rect(surface, PANEL, panel, border_radius=12)
        pygame.draw.rect(surface, CYAN, panel, 2, border_radius=12)
        title = self.big.render(self.peripheral.upper(), True, YELLOW)
        surface.blit(title, (340, 145))

        if self.peripheral == "hex":
            surface.blit(self.font.render("Message to scroll on HEX display:", True, WHITE), (340, 220))
            self.hex_msg.draw(surface, self.font)
            surface.blit(self.small.render("Output example: DISPLAY HEX ECE3073", True, GRAY), (340, 280))
        elif self.peripheral == "ledmodule":
            surface.blit(self.font.render("LED module colors:", True, WHITE), (340, 190))
            self.use_red.draw(surface, self.font)
            self.use_green.draw(surface, self.font)
            self.use_yellow.draw(surface, self.font)
            self.module_blinking.draw(surface, self.font)
            surface.blit(self.font.render("Blink seconds:", True, WHITE), (340, 325))
            self.ledmodule_blink_sec.draw(surface, self.font)
            surface.blit(self.small.render("Output example: RED LED MODULE BLINKING EVERY 5 SECOND", True, GRAY), (340, 365))
        elif self.peripheral == "led":
            surface.blit(self.font.render("Number of FPGA LEDs:", True, WHITE), (340, 230))
            self.led_count.draw(surface, self.font)
            self.led_from_left.draw(surface, self.font)
            self.led_blinking.draw(surface, self.font)
            surface.blit(self.font.render("Blink seconds:", True, WHITE), (340, 370))
            self.led_blink_sec.draw(surface, self.font)
        else:
            surface.blit(self.font.render("Speaker frequency in Hz:", True, WHITE), (340, 230))
            self.speaker_freq.draw(surface, self.font)
            self.speaker_has_duration.draw(surface, self.font)
            surface.blit(self.font.render("Duration seconds:", True, WHITE), (340, 325))
            self.speaker_duration.draw(surface, self.font)

    def draw_instruction_list(self, surface):
        rect = pygame.Rect(315, 420, 735, 250)
        pygame.draw.rect(surface, (10, 10, 18), rect, border_radius=12)
        pygame.draw.rect(surface, GRAY, rect, 2, border_radius=12)
        surface.blit(self.font.render("Instruction list before fixed-column conversion:", True, CYAN), (335, 438))
        y = 470
        if not self.instructions:
            surface.blit(self.small.render("No instructions yet.", True, GRAY), (335, y))
        else:
            for i, line in enumerate(self.instructions[-8:], 1):
                surface.blit(self.small.render(f"{i}. {line[:86]}", True, WHITE), (335, y))
                y += 23

        start_line = build_start_calibration_line(
            self.app.state.settings.max_chars_per_line,
            self.app.state.settings.start_calibration_prefix,
        )
        note = self.small.render(f"DONE converts into {self.app.state.settings.max_chars_per_line}-column rows after {start_line}.", True, YELLOW)
        surface.blit(note, (335, 642))

    def draw(self, surface):
        surface.fill(BLACK)
        draw_retro_frame(surface)
        surface.blit(self.big.render("DEBUG MENU: INSTRUCTION BUILDER", True, YELLOW), (45, 52))
        surface.blit(self.small.render("Old debug builder retained. Hardware debug: KEY0 image capture, KEY1 use image as draw background.", True, GRAY), (47, 88))
        self.draw_selector_buttons(surface)
        self.draw_current_panel(surface)
        self.draw_instruction_list(surface)

# ============================================================
# Live scroll engine for split-screen Snake/Draw
# ============================================================


class LiveScrollEngine:
    def __init__(self, app, protocol_name: str = "debug"):
        self.app = app
        self.protocol_name = protocol_name
        self.rows: List[Dict[str, object]] = []
        self.started = False
        self.font: Optional[pygame.font.Font] = None
        self.font_signature = None
        self.colors = [RED, GREEN, BLUE, YELLOW, PURPLE, CYAN]

    def reset(self):
        self.rows.clear()
        self.started = False
        self.last_raw_row = ""
        self.last_row_was_coord = False

    def start(self):
        self.reset()
        self.started = True
        self.append_rows([REALTIME_CALIB_ROW])

    def ensure_font(self):
        s = self.app.state.settings
        sig = (s.font_family, s.font_size, s.font_weight_bold)
        if self.font is None or sig != self.font_signature:
            self.font_signature = sig
            self.font = pygame.font.SysFont(s.font_family, s.font_size, bold=s.font_weight_bold)

    def row_spacing(self) -> int:
        self.ensure_font()
        assert self.font is not None
        return self.font.get_linesize() + self.app.state.settings.line_gap

    def append_rows(self, rows: List[str]):
        if not rows:
            return
        self.ensure_font()
        s = self.app.state.settings
        spacing = self.row_spacing()
        if self.rows:
            last_y = max(float(row["y"]) for row in self.rows)
            next_y = max(s.phone_height + s.margin, last_y + spacing)
        else:
            next_y = s.phone_height + s.margin

        for row in rows:
            raw = row.strip().upper()

            # QOL: when a run of coordinates changes into a new command/state
            # such as RED -> GREEN or WALL -> APPLE, leave two extra row gaps.
            # This is visual blank space only, not a blank OCR instruction.
            if (
                self.last_row_was_coord and
                is_realtime_state_row(raw) and
                self.last_raw_row != REALTIME_CALIB_ROW
            ):
                next_y += spacing * 2

            fixed = pad_row(row, REALTIME_CHARS_PER_LINE)
            self.rows.append({"text": fixed, "y": next_y})
            print(f"[LIVE QUEUE] {repr(fixed)}")
            next_y += spacing

            self.last_raw_row = raw
            self.last_row_was_coord = is_realtime_coord_row(raw)

    def update(self, dt: float):
        if not self.started:
            return
        self.ensure_font()
        s = self.app.state.settings
        assert self.font is not None
        line_h = self.font.get_linesize()
        move = speed_for_protocol(s, self.protocol_name) * min(dt, 0.05)
        for row in self.rows:
            row["y"] = float(row["y"]) - move
        self.rows = [row for row in self.rows if float(row["y"]) > -s.margin - line_h]

    def draw(self, surface, origin_x: int, origin_y: int):
        self.ensure_font()
        s = self.app.state.settings
        assert self.font is not None
        viewport = pygame.Rect(origin_x, origin_y, s.phone_width, s.phone_height)
        bg = WHITE if s.bg_color_name == "white" else BLACK
        pygame.draw.rect(surface, bg, viewport)

        sample = REALTIME_CALIB_ROW
        slot_w = max(self.font.size(ch)[0] for ch in sample) + s.char_gap
        line_width = (REALTIME_CHARS_PER_LINE - 1) * slot_w + max(self.font.size(ch)[0] for ch in sample)
        start_x = origin_x + ((s.phone_width - line_width) / 2 if s.center_text else s.left_right_padding)

        clip_old = surface.get_clip()
        surface.set_clip(viewport)
        for row in self.rows:
            text = str(row["text"])
            y = origin_y + float(row["y"])
            for col, ch in enumerate(text[:REALTIME_CHARS_PER_LINE].ljust(REALTIME_CHARS_PER_LINE)):
                char_w = self.font.size(ch)[0]
                x = start_x + col * slot_w + char_w / 2
                color = random.choice(self.colors) if s.random_color else BLACK
                if -200 <= y - origin_y <= s.phone_height + 200:
                    img = self.font.render(ch, True, color)
                    surface.blit(img, img.get_rect(center=(x, y)))
        surface.set_clip(clip_old)

        pygame.draw.rect(surface, CYAN, viewport, 2)
        overlay = pygame.font.SysFont("consolas", 16, bold=True)
        status = "RUNNING" if self.started else "PRESS START"
        msg = f"LIVE SCROLL | {status} | no loop, rows append only when you paint"
        surface.blit(overlay.render(msg, True, CYAN), (origin_x + 14, origin_y + 16))

# ============================================================
# Split-screen live grid base
# ============================================================


class LiveRealtimeGridScreen(BaseScreen):
    title = "LIVE GRID"
    cols = 10
    rows = 10
    tools: List[str] = []
    tool_colors: Dict[str, Tuple[int, int, int]] = {}
    protocol_name = "draw"

    def __init__(self, app):
        super().__init__(app)
        self.big = pygame.font.SysFont("consolas", 25, bold=True)
        self.font = pygame.font.SysFont("consolas", 17, bold=True)
        self.small = pygame.font.SysFont("consolas", 14)
        self.grid: Dict[Tuple[int, int], str] = {}
        self.current_tool = self.tools[0]
        self.last_sent_state: Optional[str] = None
        self.scroll_engine = LiveScrollEngine(app, self.protocol_name)
        self.tool_buttons: List[Button] = []
        self.control_buttons: List[Button] = []
        self.grid_rect = pygame.Rect(0, 0, 1, 1)
        self.rebuild_layout()

    def rebuild_layout(self):
        left_w = self.app.state.settings.phone_width
        left_h = self.app.state.settings.phone_height
        button_w = 118
        button_h = 34
        x = 18
        y = 50
        self.tool_buttons = []
        for tool in self.tools:
            color = self.tool_colors.get(tool, PANEL_2)
            fg = BLACK if tool in ("GREEN", "YELLOW", "WHITE") else WHITE
            self.tool_buttons.append(Button((x, y, button_w, button_h), tool, lambda t=tool: self.set_tool(t), color, fg))
            x += button_w + 8
            if x + button_w > left_w - 18:
                x = 18
                y += button_h + 8

        self.tool_area_bottom = max((b.rect.bottom for b in self.tool_buttons), default=86)

        y_buttons = left_h - 64
        self.control_buttons = [
            Button((18, y_buttons, 120, 42), "START", self.start_session, GREEN, BLACK),
            Button((150, y_buttons, 130, 42), "RESET SCROLL", self.reset_scroll, YELLOW, BLACK),
            Button((292, y_buttons, 120, 42), "SEND MAP", self.send_full_map, CYAN, BLACK),
            Button((424, y_buttons, 110, 42), "CLEAR ALL", self.clear_all, RED, BLACK),
            Button((left_w - 128, y_buttons, 110, 42), "BACK", lambda: self.app.switch("main")),
        ]
        self.buttons = self.tool_buttons + self.control_buttons

        grid_x = 22
        grid_y = self.tool_area_bottom + 66
        grid_w = left_w - 44
        grid_h = left_h - grid_y - 92
        self.grid_rect = pygame.Rect(grid_x, grid_y, grid_w, grid_h)

    def set_tool(self, tool: str):
        self.current_tool = tool

    def start_session(self):
        self.last_sent_state = None
        self.scroll_engine.start()
        self.app.state.protocol = self.protocol_name
        self.app.state.raw_rows = []
        self.app.state.scroll_text = realtime_columnize([])

    def prepare_for_panel_entry(self):
        # VGA/IMG/Arduino reset panel calibration after SW9 escape/menu.
        # Every time the user enters a realtime panel, show X99Y99 exactly once
        # to reacquire slot positions. After that, paint commands append normally.
        self.start_session()

    def reset_scroll(self):
        self.last_sent_state = None
        self.scroll_engine.reset()

    def clear_all(self):
        self.grid.clear()
        if self.protocol_name == "snake":
            self.queue_instruction_rows(["CLEAR"], reset_state=True)
        else:
            # For Draw Pixel, CLEAR is still useful as a visual reset marker if IMG/VGA supports it later.
            self.queue_instruction_rows(["CLEAR"], reset_state=True)

    def send_full_map(self):
        if not self.scroll_engine.started:
            self.start_session()
        priority = [tool for tool in self.tools if tool != "ERASE"]
        rows = compress_realtime_paint_rows(self.grid, priority)
        self.queue_instruction_rows(rows, reset_state=True)

    def queue_instruction_rows(self, rows: List[str], reset_state=False):
        if not rows:
            return
        if not self.scroll_engine.started:
            # Do not silently lose the command. Start calibration first, then append the command.
            self.start_session()
        if reset_state:
            self.last_sent_state = None
        self.scroll_engine.append_rows(rows)
        self.app.state.protocol = self.protocol_name
        self.app.state.raw_rows.extend(rows)
        self.app.state.scroll_text = realtime_columnize(self.app.state.raw_rows)

    def cell_from_mouse(self, pos) -> Optional[Tuple[int, int]]:
        if not self.grid_rect.collidepoint(pos):
            return None
        cell_w = self.grid_rect.width / self.cols
        cell_h = self.grid_rect.height / self.rows
        x = int((pos[0] - self.grid_rect.x) / cell_w)
        y = int((pos[1] - self.grid_rect.y) / cell_h)
        if 0 <= x < self.cols and 0 <= y < self.rows:
            return x, y
        return None

    def paint_at(self, pos):
        cell = self.cell_from_mouse(pos)
        if cell is None:
            return

        old_value = self.grid.get(cell)
        if self.current_tool == "ERASE":
            if old_value is None:
                return
            self.grid.pop(cell, None)
        else:
            if old_value == self.current_tool:
                return
            self.grid[cell] = self.current_tool

        rows_to_send: List[str] = []
        if self.current_tool != self.last_sent_state:
            rows_to_send.append(self.current_tool)
            self.last_sent_state = self.current_tool
        rows_to_send.append(coord_row(cell[0], cell[1]))
        self.queue_instruction_rows(rows_to_send)

    def handle_event(self, event):
        # Rebuild layout after settings changed and the window was resized.
        self.rebuild_layout()
        super().handle_event(event)
        if event.type == pygame.KEYDOWN:
            if event.key == pygame.K_SPACE:
                self.start_session()
            elif event.key == pygame.K_r:
                self.reset_scroll()
            elif event.key == pygame.K_c:
                self.clear_all()
            elif event.key == pygame.K_m:
                self.send_full_map()
        elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            self.paint_at(event.pos)
        elif event.type == pygame.MOUSEMOTION and pygame.mouse.get_pressed()[0]:
            self.paint_at(event.pos)

    def update(self, dt: float):
        self.scroll_engine.update(dt)

    def draw_grid(self, surface):
        pygame.draw.rect(surface, PANEL, self.grid_rect, border_radius=10)
        cell_w = self.grid_rect.width / self.cols
        cell_h = self.grid_rect.height / self.rows
        for y in range(self.rows):
            for x in range(self.cols):
                item = self.grid.get((x, y))
                if item:
                    color = self.tool_colors.get(item, WHITE)
                    rx = int(self.grid_rect.x + x * cell_w)
                    ry = int(self.grid_rect.y + y * cell_h)
                    rw = max(1, int(cell_w))
                    rh = max(1, int(cell_h))
                    pygame.draw.rect(surface, color, (rx + 1, ry + 1, rw - 2, rh - 2))
        for x in range(self.cols + 1):
            px = int(self.grid_rect.x + x * cell_w)
            pygame.draw.line(surface, (55, 55, 72), (px, self.grid_rect.y), (px, self.grid_rect.bottom))
        for y in range(self.rows + 1):
            py = int(self.grid_rect.y + y * cell_h)
            pygame.draw.line(surface, (55, 55, 72), (self.grid_rect.x, py), (self.grid_rect.right, py))
        pygame.draw.rect(surface, CYAN, self.grid_rect, 2, border_radius=10)

    def draw_left_panel(self, surface):
        left_w = self.app.state.settings.phone_width
        left_h = self.app.state.settings.phone_height
        pygame.draw.rect(surface, BLACK, (0, 0, left_w, left_h))
        pygame.draw.rect(surface, CYAN, (8, 8, left_w - 16, left_h - 16), 3, border_radius=10)
        surface.blit(self.big.render(self.title, True, YELLOW), (22, 18))
        help_line = "X99Y99 appears once on entry/START. Paint after that = realtime rows. C=clear, M=send map."
        help_y = getattr(self, "tool_area_bottom", 86) + 14
        surface.blit(self.small.render(help_line, True, GRAY), (22, help_y))

        for button in self.buttons:
            button.draw(surface, self.font)

        try:
            tool_index = self.tools.index(self.current_tool)
            selected = self.tool_buttons[tool_index].rect.inflate(6, 6)
            pygame.draw.rect(surface, CYAN, selected, 3, border_radius=9)
        except ValueError:
            pass

        self.draw_grid(surface)
        status = "RUNNING" if self.scroll_engine.started else "WAITING FOR START"
        line = f"Tool={self.current_tool} | Cells={len(self.grid)} | Last state={self.last_sent_state or 'NONE'} | {status}"
        surface.blit(self.font.render(line, True, CYAN), (22, self.grid_rect.bottom + 18))
        note = "CLEAR ALL sends the 6-char row CLEAR. Individual erasing sends ERASE + coordinate."
        surface.blit(self.small.render(note, True, GRAY), (22, self.grid_rect.bottom + 45))

    def draw(self, surface):
        self.rebuild_layout()
        s = self.app.state.settings
        surface.fill(BLACK)
        self.draw_left_panel(surface)
        self.scroll_engine.draw(surface, s.phone_width, 0)
        pygame.draw.line(surface, PURPLE, (s.phone_width, 0), (s.phone_width, s.phone_height), 4)


class SnakeLiveScreen(LiveRealtimeGridScreen):
    title = "SNAKE LIVE REALTIME"
    cols = SNAKE_COLS
    rows = SNAKE_ROWS
    tools = ["WALL", "APPLE", "PORTA", "PORTB", "ERASE"]
    tool_colors = SNAKE_TOOL_RGB
    protocol_name = "snake"


class DrawLiveScreen(LiveRealtimeGridScreen):
    title = "DRAW PIXEL LIVE REALTIME"
    cols = DRAW_COLS
    rows = DRAW_ROWS
    tools = ["RED", "GREEN", "BLUE", "YELLOW", "BLACK", "WHITE", "ERASE"]
    tool_colors = {**DRAW_COLOR_RGB, "ERASE": DARK}
    protocol_name = "draw"

    def rebuild_layout(self):
        left_w = self.app.state.settings.phone_width
        left_h = self.app.state.settings.phone_height
        button_w = 102
        button_h = 32
        x = 18
        y = 50
        self.tool_buttons = []
        for tool in self.tools:
            color = self.tool_colors.get(tool, PANEL_2)
            fg = BLACK if tool in ("GREEN", "YELLOW", "WHITE") else WHITE
            self.tool_buttons.append(Button((x, y, button_w, button_h), tool, lambda t=tool: self.set_tool(t), color, fg))
            x += button_w + 8
            if x + button_w > left_w - 18:
                x = 18
                y += button_h + 8

        self.tool_area_bottom = max((b.rect.bottom for b in self.tool_buttons), default=86)

        y_buttons = left_h - 60
        self.control_buttons = [
            Button((18, y_buttons, 110, 40), "START", self.start_session, GREEN, BLACK),
            Button((140, y_buttons, 120, 40), "RST SCROLL", self.reset_scroll, YELLOW, BLACK),
            Button((272, y_buttons, 110, 40), "SEND MAP", self.send_full_map, CYAN, BLACK),
            Button((394, y_buttons, 110, 40), "CLEAR ALL", self.clear_all, RED, BLACK),
            Button((left_w - 128, y_buttons, 110, 40), "BACK", lambda: self.app.switch("main")),
        ]
        self.buttons = self.tool_buttons + self.control_buttons

        size = min(left_w - 56, left_h - self.tool_area_bottom - 150)
        size = max(384, size)
        grid_x = (left_w - size) // 2
        grid_y = self.tool_area_bottom + 54
        self.grid_rect = pygame.Rect(grid_x, grid_y, size, size)

    def draw_grid(self, surface):
        pygame.draw.rect(surface, PANEL, self.grid_rect, border_radius=10)
        cell_w = self.grid_rect.width / self.cols
        cell_h = self.grid_rect.height / self.rows

        for (x, y), item in self.grid.items():
            color = self.tool_colors.get(item, WHITE)
            rx = int(self.grid_rect.x + x * cell_w)
            ry = int(self.grid_rect.y + y * cell_h)
            rx2 = int(self.grid_rect.x + (x + 1) * cell_w)
            ry2 = int(self.grid_rect.y + (y + 1) * cell_h)
            rw = max(1, rx2 - rx)
            rh = max(1, ry2 - ry)
            pygame.draw.rect(surface, color, (rx, ry, rw, rh))

        for x in range(0, self.cols + 1, 8):
            px = int(self.grid_rect.x + x * cell_w)
            pygame.draw.line(surface, (62, 62, 82), (px, self.grid_rect.y), (px, self.grid_rect.bottom))
        for y in range(0, self.rows + 1, 8):
            py_line = int(self.grid_rect.y + y * cell_h)
            pygame.draw.line(surface, (62, 62, 82), (self.grid_rect.x, py_line), (self.grid_rect.right, py_line))

        pygame.draw.rect(surface, CYAN, self.grid_rect, 2, border_radius=10)

    def draw_left_panel(self, surface):
        left_w = self.app.state.settings.phone_width
        left_h = self.app.state.settings.phone_height
        pygame.draw.rect(surface, BLACK, (0, 0, left_w, left_h))
        pygame.draw.rect(surface, CYAN, (8, 8, left_w - 16, left_h - 16), 3, border_radius=10)
        surface.blit(self.big.render(self.title, True, YELLOW), (22, 18))
        help_line = "DRAW is now 96x96. X99Y99 appears once on entry/START. Paint after that = realtime rows."
        help_y = getattr(self, "tool_area_bottom", 86) + 12
        surface.blit(self.small.render(help_line, True, GRAY), (22, help_y))

        for button in self.buttons:
            button.draw(surface, self.font)

        try:
            tool_index = self.tools.index(self.current_tool)
            selected = self.tool_buttons[tool_index].rect.inflate(6, 6)
            pygame.draw.rect(surface, CYAN, selected, 3, border_radius=9)
        except ValueError:
            pass

        self.draw_grid(surface)
        status = "RUNNING" if self.scroll_engine.started else "WAITING FOR START"
        line = f"Tool={self.current_tool} | Pixels={len(self.grid)} | Last={self.last_sent_state or 'NONE'} | {status}"
        surface.blit(self.font.render(line, True, CYAN), (22, self.grid_rect.bottom + 14))
        note = "Exact VGA draw resolution: 96x96. Debug KEY1 background transfer is now 1:1."
        surface.blit(self.small.render(note, True, GRAY), (22, self.grid_rect.bottom + 38))


class BattleshipLiveScreen(LiveRealtimeGridScreen):
    """Additive Battleship designer.

    This screen intentionally keeps the same live split-screen architecture as
    Snake/Draw. It only adds a 10x10 ship designer and Battleship-specific rows:
    CLEAR, SHIP, ERASE, DONE, X##Y##.
    """

    title = "BATTLESHIP DESIGNER LIVE"
    cols = BATTLE_COLS
    rows = BATTLE_ROWS
    tools = ["SHIP", "ERASE"]
    tool_colors = BATTLE_TOOL_RGB
    protocol_name = "battle"

    def __init__(self, app):
        self.ship_orientation_horizontal = True
        self.fleet_index = 0
        self.fleet_sizes = list(BATTLE_FLEET_SIZES)
        self.preview_cells: List[Tuple[int, int]] = []
        super().__init__(app)

    def rebuild_layout(self):
        left_w = self.app.state.settings.phone_width
        left_h = self.app.state.settings.phone_height
        button_w = 118
        button_h = 34
        x = 18
        y = 50
        self.tool_buttons = []
        for tool in self.tools:
            color = self.tool_colors.get(tool, PANEL_2)
            self.tool_buttons.append(Button((x, y, button_w, button_h), tool, lambda t=tool: self.set_tool(t), color, WHITE))
            x += button_w + 8

        y_buttons = left_h - 64
        self.control_buttons = [
            Button((18, y_buttons, 120, 42), "START", self.start_session, GREEN, BLACK),
            Button((150, y_buttons, 130, 42), "RESET SCROLL", self.reset_scroll, YELLOW, BLACK),
            Button((292, y_buttons, 130, 42), "SEND FLEET", self.send_full_map, CYAN, BLACK),
            Button((434, y_buttons, 110, 42), "CLEAR ALL", self.clear_all, RED, BLACK),
            Button((556, y_buttons, 120, 42), "ROTATE", self.rotate_ship, PURPLE, WHITE),
            Button((left_w - 128, y_buttons, 110, 42), "BACK", lambda: self.app.switch("main")),
        ]
        self.buttons = self.tool_buttons + self.control_buttons
        self.tool_area_bottom = max((b.rect.bottom for b in self.tool_buttons), default=86)

        # Make the 10x10 grid large and graphic, not stretched too awkwardly.
        # Keep it below the header/help area so text never collides with buttons.
        size = min(left_w - 90, left_h - 255)
        grid_x = 46
        grid_y = self.tool_area_bottom + 86
        self.grid_rect = pygame.Rect(grid_x, grid_y, size, size)

    def rotate_ship(self):
        self.ship_orientation_horizontal = not self.ship_orientation_horizontal

    def start_session(self):
        self.last_sent_state = None
        self.scroll_engine.start()
        self.app.state.protocol = self.protocol_name
        self.app.state.raw_rows = []
        self.app.state.scroll_text = realtime_columnize([])
        # For Battleship, START should send the current hidden layout after calibration.
        self.queue_instruction_rows(self.build_full_layout_rows(), reset_state=True)

    def clear_all(self):
        self.grid.clear()
        self.fleet_index = 0
        self.last_sent_state = None
        self.queue_instruction_rows(["CLEAR"], reset_state=True)

    def send_full_map(self):
        if not self.scroll_engine.started:
            self.start_session()
            return
        self.queue_instruction_rows(self.build_full_layout_rows(), reset_state=True)

    def build_full_layout_rows(self) -> List[str]:
        rows: List[str] = ["CLEAR"]
        coords = sorted(self.grid.keys(), key=lambda p: (p[1], p[0]))
        if coords:
            rows.append("SHIP")
            for x, y in coords:
                rows.append(coord_row(x, y))
        rows.append("DONE")
        return rows

    def next_ship_size(self) -> int:
        if self.fleet_index < len(self.fleet_sizes):
            return self.fleet_sizes[self.fleet_index]
        return 0

    def total_allowed_ship_cells(self) -> int:
        return sum(self.fleet_sizes)

    def used_ship_cells(self) -> int:
        return len(self.grid)

    def remaining_ship_cells(self) -> int:
        return max(0, self.total_allowed_ship_cells() - self.used_ship_cells())

    def ship_cells_from_anchor(self, x: int, y: int, size: Optional[int] = None) -> List[Tuple[int, int]]:
        ship_len = size if size is not None else self.next_ship_size()
        cells = []
        for i in range(ship_len):
            cx = x + i if self.ship_orientation_horizontal else x
            cy = y if self.ship_orientation_horizontal else y + i
            cells.append((cx, cy))
        return cells

    def can_place_ship(self, cells: List[Tuple[int, int]]) -> bool:
        for cx, cy in cells:
            if cx < 0 or cx >= self.cols or cy < 0 or cy >= self.rows:
                return False
            if (cx, cy) in self.grid:
                return False
        return True

    def queue_state_coord(self, state: str, cell: Tuple[int, int]):
        rows_to_send: List[str] = []
        if state != self.last_sent_state:
            rows_to_send.append(state)
            self.last_sent_state = state
        rows_to_send.append(coord_row(cell[0], cell[1]))
        self.queue_instruction_rows(rows_to_send)

    def paint_at(self, pos):
        cell = self.cell_from_mouse(pos)
        if cell is None:
            return

        if self.current_tool == "ERASE":
            if cell in self.grid:
                self.grid.pop(cell, None)
                self.queue_state_coord("ERASE", cell)
            return

        if self.fleet_index >= len(self.fleet_sizes):
            print("[BATTLE PY] Fleet limit reached. Erase or CLEAR ALL before placing more ships.")
            return

        ship_cells = self.ship_cells_from_anchor(cell[0], cell[1])
        if len(ship_cells) > self.remaining_ship_cells():
            print("[BATTLE PY] Not enough ship-cell budget left.")
            return

        if not self.can_place_ship(ship_cells):
            return

        for ship_cell in ship_cells:
            self.grid[ship_cell] = "SHIP"
            self.queue_state_coord("SHIP", ship_cell)

        self.fleet_index += 1

    def handle_event(self, event):
        self.rebuild_layout()
        super().handle_event(event)
        if event.type == pygame.KEYDOWN:
            if event.key == pygame.K_o:
                self.rotate_ship()
            elif event.key == pygame.K_BACKSPACE:
                self.current_tool = "ERASE"
        elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 3:
            old_tool = self.current_tool
            self.current_tool = "ERASE"
            self.paint_at(event.pos)
            self.current_tool = old_tool

    def draw_ocean_backplate(self, surface):
        rect = self.grid_rect.inflate(32, 32)
        pygame.draw.rect(surface, (6, 18, 44), rect.move(5, 5), border_radius=18)
        pygame.draw.rect(surface, (12, 45, 92), rect, border_radius=18)
        pygame.draw.rect(surface, CYAN, rect, 3, border_radius=18)
        for i in range(0, rect.height, 18):
            y = rect.y + i
            pygame.draw.line(surface, (35, 90, 145), (rect.x + 12, y), (rect.right - 12, y), 1)

    def draw_grid(self, surface):
        self.draw_ocean_backplate(surface)
        cell_w = self.grid_rect.width / self.cols
        cell_h = self.grid_rect.height / self.rows
        mouse_cell = self.cell_from_mouse(pygame.mouse.get_pos())
        preview = set()
        if mouse_cell and self.current_tool == "SHIP":
            cells = self.ship_cells_from_anchor(mouse_cell[0], mouse_cell[1])
            if self.can_place_ship(cells):
                preview = set(cells)

        for y in range(self.rows):
            for x in range(self.cols):
                rx = int(self.grid_rect.x + x * cell_w)
                ry = int(self.grid_rect.y + y * cell_h)
                rw = max(1, int(cell_w))
                rh = max(1, int(cell_h))
                water = (16, 52, 100) if (x + y) % 2 == 0 else (12, 42, 82)
                pygame.draw.rect(surface, water, (rx, ry, rw - 1, rh - 1), border_radius=6)
                pygame.draw.rect(surface, (65, 145, 215), (rx, ry, rw - 1, rh - 1), 1, border_radius=6)

                if (x, y) in preview:
                    pygame.draw.rect(surface, (90, 255, 190), (rx + 3, ry + 3, rw - 7, rh - 7), border_radius=7)

                if self.grid.get((x, y)) == "SHIP":
                    # Pixel-metal anime ship tile: shadow, hull, highlight, porthole.
                    pygame.draw.rect(surface, (0, 0, 0), (rx + 5, ry + 6, rw - 8, rh - 8), border_radius=8)
                    pygame.draw.rect(surface, (170, 185, 210), (rx + 3, ry + 3, rw - 7, rh - 7), border_radius=8)
                    pygame.draw.rect(surface, (230, 238, 255), (rx + 6, ry + 6, rw - 13, max(3, rh // 5)), border_radius=4)
                    pygame.draw.rect(surface, (80, 90, 115), (rx + 6, ry + rh - 12, rw - 13, 5), border_radius=4)
                    pygame.draw.circle(surface, CYAN, (rx + rw // 2, ry + rh // 2), max(3, min(rw, rh) // 8))
                    pygame.draw.rect(surface, WHITE, (rx + 3, ry + 3, rw - 7, rh - 7), 2, border_radius=8)

        pygame.draw.rect(surface, YELLOW, self.grid_rect, 3, border_radius=10)

    def draw_left_panel(self, surface):
        left_w = self.app.state.settings.phone_width
        left_h = self.app.state.settings.phone_height
        pygame.draw.rect(surface, BLACK, (0, 0, left_w, left_h))
        pygame.draw.rect(surface, CYAN, (8, 8, left_w - 16, left_h - 16), 3, border_radius=10)
        pygame.draw.rect(surface, (18, 18, 44), (16, 16, left_w - 32, 116), border_radius=14)
        pygame.draw.rect(surface, PURPLE, (16, 16, left_w - 32, 116), 2, border_radius=14)

        surface.blit(self.big.render(self.title, True, YELLOW), (24, 20))
        help_y = getattr(self, "tool_area_bottom", 86) + 12
        surface.blit(self.small.render("Design hidden fleet here; VGA Battleship plays it. X99Y99 appears once on entry.", True, CYAN), (24, help_y))
        surface.blit(self.small.render("O/ROTATE = rotate | right click = erase | SEND FLEET = resend layout", True, GRAY), (24, help_y + 20))

        for button in self.buttons:
            button.draw(surface, self.font)

        try:
            tool_index = self.tools.index(self.current_tool)
            selected = self.tool_buttons[tool_index].rect.inflate(6, 6)
            pygame.draw.rect(surface, CYAN, selected, 3, border_radius=9)
        except ValueError:
            pass

        self.draw_grid(surface)

        orient = "HORIZONTAL" if self.ship_orientation_horizontal else "VERTICAL"
        next_size = self.next_ship_size()
        fleet_left = self.fleet_sizes[self.fleet_index:] if self.fleet_index < len(self.fleet_sizes) else []
        status = "RUNNING" if self.scroll_engine.started else "WAITING FOR START"
        used = self.used_ship_cells()
        total = self.total_allowed_ship_cells()
        next_label = str(next_size) if next_size > 0 else "DONE"
        line = f"Tool={self.current_tool} | Ship cells={used}/{total} | Next={next_label} | {orient} | {status}"
        surface.blit(self.font.render(line, True, CYAN), (24, self.grid_rect.bottom + 24))
        fleet_msg = f"Fleet pieces left: {fleet_left if fleet_left else 'none'}  | Limit: {len(BATTLE_FLEET_SIZES)} ships / {total} cells"
        surface.blit(self.small.render(fleet_msg, True, WHITE), (24, self.grid_rect.bottom + 52))
        surface.blit(self.small.render("VGA: accel aim, KEY0 fire, KEY1 reset, SW5/SW6/SW7 bombs, SW8 reveal, SW9 menu.", True, GRAY), (24, self.grid_rect.bottom + 76))


# ============================================================
# Settings screen
# ============================================================


class SettingsScreen(BaseScreen):
    def __init__(self, app):
        super().__init__(app)
        self.big = pygame.font.SysFont("consolas", 28, bold=True)
        self.font = pygame.font.SysFont("consolas", 18)
        self.small = pygame.font.SysFont("consolas", 15)
        s = app.state.settings
        self.inputs = {
            "phone_width": TextInput((420, 122, 120, 34), str(s.phone_width), "800", 5),
            "phone_height": TextInput((420, 164, 120, 34), str(s.phone_height), "844", 5),
            "frequency": TextInput((420, 206, 120, 34), str(s.frequency), "90", 4),
            "font_size": TextInput((420, 248, 120, 34), str(s.font_size), "70", 4),
            "char_gap": TextInput((420, 290, 120, 34), str(s.char_gap), "20", 4),
            "line_gap": TextInput((420, 332, 120, 34), str(s.line_gap), "75", 4),
            "margin": TextInput((420, 374, 120, 34), str(s.margin), "50", 4),
            "left_right_padding": TextInput((420, 416, 120, 34), str(s.left_right_padding), "120", 4),
            "max_chars_per_line": TextInput((420, 458, 120, 34), str(s.max_chars_per_line), "8", 3),
            "start_calibration_prefix": TextInput((420, 500, 180, 34), s.start_calibration_prefix, "START", 10),

            # Per-menu speeds. Defaults:
            # debug = 250, coordinate games = 400.
            "debug_speed_pps": TextInput((790, 122, 120, 34), str(s.debug_speed_pps), "250", 5),
            "snake_speed_pps": TextInput((790, 164, 120, 34), str(s.snake_speed_pps), "400", 5),
            "draw_speed_pps": TextInput((790, 206, 120, 34), str(s.draw_speed_pps), "300", 5),
            "battle_speed_pps": TextInput((790, 248, 120, 34), str(s.battle_speed_pps), "400", 5),
        }
        self.random_color = Toggle((670, 310, 220, 28), "Random colors", s.random_color)
        self.center_text = Toggle((670, 352, 220, 28), "Center text", s.center_text)
        self.calibration = Toggle((670, 394, 260, 28), "Enable START calibration", s.enable_start_calibration_line)
        self.horizontal = Toggle((670, 436, 260, 28), "Horizontal mode", s.mode == "horizontal")
        self.reverse = Toggle((670, 478, 260, 28), "Reverse direction", False)
        self.buttons = [
            Button((60, 610, 210, 44), "APPLY", self.apply, GREEN, BLACK),
            Button((300, 610, 210, 44), "RESTORE ORIGINAL", self.restore_original, YELLOW, BLACK),
            Button((540, 610, 210, 44), "BACK", lambda: self.app.switch("main")),
        ]

    def int_from(self, name, default, min_value=1, max_value=None):
        try:
            value = int(self.inputs[name].text.strip())
        except ValueError:
            value = default
        value = max(min_value, value)
        if max_value is not None:
            value = min(max_value, value)
        return value

    def apply(self):
        s = self.app.state.settings
        s.phone_width = self.int_from("phone_width", SCROLL_WIDTH, 300, 1600)
        s.phone_height = self.int_from("phone_height", SCROLL_HEIGHT, 300, 1600)
        s.frequency = self.int_from("frequency", 90, 1, 120)
        s.debug_speed_pps = self.int_from("debug_speed_pps", DEFAULT_DEBUG_SPEED_PPS, 1, 10000)
        s.snake_speed_pps = self.int_from("snake_speed_pps", DEFAULT_COORD_GAME_SPEED_PPS, 1, 10000)
        s.draw_speed_pps = self.int_from("draw_speed_pps", DEFAULT_DRAW_SPEED_PPS, 1, 10000)
        s.battle_speed_pps = self.int_from("battle_speed_pps", DEFAULT_COORD_GAME_SPEED_PPS, 1, 10000)
        s.speed_pps = s.debug_speed_pps  # legacy/debug fallback
        s.font_size = self.int_from("font_size", 70, 1, 300)
        s.char_gap = self.int_from("char_gap", 20, 0, 200)
        s.line_gap = self.int_from("line_gap", 75, 0, 500)
        s.margin = self.int_from("margin", 50, 0, 1000)
        s.left_right_padding = self.int_from("left_right_padding", 120, 0, max(0, s.phone_width // 2 - 1))
        s.max_chars_per_line = self.int_from("max_chars_per_line", 8, 1, 64)
        s.start_calibration_prefix = self.inputs["start_calibration_prefix"].text.strip() or "START"
        s.random_color = self.random_color.value
        s.center_text = self.center_text.value
        s.enable_start_calibration_line = self.calibration.value
        if self.horizontal.value:
            s.mode = "horizontal"
            s.direction = "left_to_right" if self.reverse.value else "right_to_left"
        else:
            s.mode = "vertical"
            s.direction = "top_to_down" if self.reverse.value else "bottom_to_top"
        if self.app.state.protocol == "debug" and self.app.state.raw_instruction_text:
            self.app.set_debug_text(self.app.state.raw_instruction_text)

    def restore_original(self):
        self.app.state.settings = ScrollSettings()
        self.app.screens["settings"] = SettingsScreen(self.app)

    def handle_event(self, event):
        super().handle_event(event)
        for widget in list(self.inputs.values()) + [self.random_color, self.center_text, self.calibration, self.horizontal, self.reverse]:
            widget.handle_event(event)

    def draw(self, surface):
        surface.fill(BLACK)
        draw_retro_frame(surface)
        surface.blit(self.big.render("SCROLL SETTINGS", True, YELLOW), (45, 52))
        surface.blit(self.small.render("These settings affect the right live scroll panel and old debug scroll preview.", True, GRAY), (47, 88))
        labels = [
            ("phone_width", "Scroll width"),
            ("phone_height", "Scroll height"),
            ("frequency", "Frequency"),
            ("font_size", "Font size"),
            ("char_gap", "Char gap"),
            ("line_gap", "Line gap"),
            ("margin", "Reset margin"),
            ("left_right_padding", "Left/right padding"),
            ("max_chars_per_line", "Debug fixed columns"),
            ("start_calibration_prefix", "Debug calibration prefix"),

            ("debug_speed_pps", "Debug speed"),
            ("snake_speed_pps", "Snake speed"),
            ("draw_speed_pps", "Draw speed"),
            ("battle_speed_pps", "Battle speed"),
        ]
        for name, label in labels:
            inp = self.inputs[name]
            label_x = 610 if inp.rect.x >= 700 else 85
            surface.blit(self.font.render(label + ":", True, WHITE), (label_x, inp.rect.y + 7))
            inp.draw(surface, self.font)
        for toggle in [self.random_color, self.center_text, self.calibration, self.horizontal, self.reverse]:
            toggle.draw(surface, self.font)
        start = build_start_calibration_line(
            self.app.state.settings.max_chars_per_line,
            self.app.state.settings.start_calibration_prefix,
        )
        surface.blit(self.small.render(f"Current debug calibration row: {start}", True, CYAN), (670, 350))
        surface.blit(self.small.render("Defaults: debug=250, draw=300, snake/battle=400.", True, CYAN), (670, 540))
        surface.blit(self.small.render("Snake/draw/battle calibration remains X99Y99.", True, GRAY), (670, 566))
        surface.blit(self.small.render("Live split window size = 2 x scroll width by scroll height.", True, GRAY), (670, 592))
        for b in self.buttons:
            b.draw(surface, self.font)

# ============================================================
# Full scroll preview for debug / last text
# ============================================================


class ScrollScreen(BaseScreen):
    def __init__(self, app):
        super().__init__(app)
        self.font = None
        self.last_signature = None
        self.last_text = None
        self.char_items = []
        self.offset = 0.0
        self.block_left = 0
        self.block_right = 0
        self.block_top = 0
        self.block_bottom = 0
        self.colors = [RED, GREEN, BLUE, YELLOW, PURPLE, CYAN]
        self.back = Button((20, 20, 100, 36), "BACK", lambda: app.switch("main"))
        self.buttons = [self.back]

    def signature(self):
        st = self.app.state
        s = st.settings
        width = s.max_chars_per_line if st.protocol == "debug" else REALTIME_CHARS_PER_LINE
        return (
            st.scroll_text,
            s.phone_width,
            s.phone_height,
            s.frequency,
            speed_for_protocol(s, st.protocol),
            s.debug_speed_pps,
            s.snake_speed_pps,
            s.draw_speed_pps,
            s.battle_speed_pps,
            s.font_family,
            s.font_size,
            s.font_weight_bold,
            s.char_gap,
            s.line_gap,
            s.margin,
            s.random_color,
            s.mode,
            s.direction,
            s.center_text,
            s.left_right_padding,
            width,
        )

    def ensure_layout(self):
        sig = self.signature()
        if sig == self.last_signature:
            return
        self.last_signature = sig
        if self.app.state.scroll_text:
            self.last_text = self.app.state.scroll_text
        else:
            self.last_text = realtime_columnize(["RED", coord_row(1, 1)])
        self.prepare_layout(self.last_text)

    def prepare_layout(self, text: str):
        st = self.app.state
        s = st.settings
        width = max(1, int(s.max_chars_per_line)) if st.protocol == "debug" else REALTIME_CHARS_PER_LINE
        self.font = pygame.font.SysFont(s.font_family, s.font_size, bold=s.font_weight_bold)
        line_height = self.font.get_linesize()
        lines = text.split("\n")
        self.char_items = []
        total_h = len(lines) * line_height + max(0, len(lines) - 1) * s.line_gap
        start_y = (s.phone_height - total_h) / 2 + line_height / 2
        slot_sample = build_start_calibration_line(width, s.start_calibration_prefix) if st.protocol == "debug" else REALTIME_CALIB_ROW
        slot_w = max(self.font.size(ch)[0] for ch in slot_sample) + s.char_gap
        line_width = (width - 1) * slot_w + max(self.font.size(ch)[0] for ch in slot_sample)
        start_x = (s.phone_width - line_width) / 2 if s.center_text else s.left_right_padding
        self.block_left = start_x
        self.block_right = start_x + line_width
        self.block_top = start_y - line_height / 2
        self.block_bottom = start_y + (len(lines) - 1) * (line_height + s.line_gap) + line_height / 2
        for row, line in enumerate(lines):
            fixed = line[:width].ljust(width)
            y = start_y + row * (line_height + s.line_gap)
            for col, ch in enumerate(fixed):
                char_w = self.font.size(ch)[0]
                x = start_x + col * slot_w + char_w / 2
                color = random.choice(self.colors) if s.random_color else BLACK
                self.char_items.append((ch, x, y, color))
        self.reset_offset()
        self.print_scroll_text()

    def print_scroll_text(self):
        print("\n=== FINAL SCROLL ROWS ===")
        print(f"protocol={self.app.state.protocol}")
        for line in self.last_text.split("\n"):
            print(repr(line))
        print("=========================")

    def reset_offset(self):
        s = self.app.state.settings
        if s.mode == "horizontal":
            self.offset = s.phone_width - self.block_left + s.margin if s.direction == "right_to_left" else -self.block_right - s.margin
        else:
            self.offset = s.phone_height - self.block_top + s.margin if s.direction == "bottom_to_top" else -self.block_bottom - s.margin

    def update(self, dt: float):
        self.ensure_layout()
        s = self.app.state.settings
        move = speed_for_protocol(s, self.app.state.protocol) * min(dt, 0.05)
        if s.mode == "horizontal":
            if s.direction == "right_to_left":
                self.offset -= move
                if self.block_right + self.offset < -s.margin:
                    self.offset = s.phone_width - self.block_left + s.margin
            else:
                self.offset += move
                if self.block_left + self.offset > s.phone_width + s.margin:
                    self.offset = -self.block_right - s.margin
        else:
            if s.direction == "bottom_to_top":
                self.offset -= move
                if self.block_bottom + self.offset < -s.margin:
                    self.offset = s.phone_height - self.block_top + s.margin
            else:
                self.offset += move
                if self.block_top + self.offset > s.phone_height + s.margin:
                    self.offset = -self.block_bottom - s.margin

    def draw(self, surface):
        self.ensure_layout()
        st = self.app.state
        s = st.settings
        surface.fill(WHITE if s.bg_color_name == "white" else BLACK)
        for ch, bx, by, color in self.char_items:
            x, y = bx, by
            if s.mode == "horizontal":
                x += self.offset
            else:
                y += self.offset
            if -200 <= x <= s.phone_width + 200 and -200 <= y <= s.phone_height + 200:
                img = self.font.render(ch, True, color)
                surface.blit(img, img.get_rect(center=(x, y)))
        overlay = pygame.font.SysFont("consolas", 16, bold=True)
        self.back.draw(surface, overlay)
        line = f"{st.protocol.upper()} preview | {'6-col realtime' if st.protocol != 'debug' else str(s.max_chars_per_line) + '-col debug'} | ESC/BACK returns"
        surface.blit(overlay.render(line, True, CYAN), (135, 28))

# ============================================================
# App wrapper
# ============================================================


class App:
    def __init__(self):
        pygame.init()
        pygame.display.set_caption("Realtime Row Protocol Builder - Live Split")
        self.surface = pygame.display.set_mode((MENU_WIDTH, MENU_HEIGHT))
        self.clock = pygame.time.Clock()
        self.state = AppState()
        self.screens = {
            "main": MainMenuScreen(self),
            "debug": DebugScreen(self),
            "battle": BattleshipLiveScreen(self),
            "snake": SnakeLiveScreen(self),
            "draw": DrawLiveScreen(self),
            "settings": SettingsScreen(self),
            "scroll": ScrollScreen(self),
        }
        self.set_realtime_rows("draw", ["RED", coord_row(1, 1), coord_row(2, 1), "BLUE", coord_row(3, 1)])

    def resize_for_screen(self, name: str):
        s = self.state.settings
        if name in ("battle", "snake", "draw"):
            size = (s.phone_width * 2, s.phone_height)
        elif name == "scroll":
            size = (s.phone_width, s.phone_height)
        else:
            size = (MENU_WIDTH, MENU_HEIGHT)
        if self.surface.get_size() != size:
            self.surface = pygame.display.set_mode(size)

    def switch(self, name: str):
        self.state.previous_screen = self.state.screen
        self.state.screen = name
        self.resize_for_screen(name)
        if name in ("battle", "snake", "draw"):
            screen = self.screens.get(name)
            if hasattr(screen, "prepare_for_panel_entry"):
                screen.prepare_for_panel_entry()

    def set_debug_text(self, raw_text: str):
        self.state.protocol = "debug"
        self.state.raw_instruction_text = raw_text
        self.state.raw_rows = raw_text.split("\n")
        self.state.scroll_text = fixed_columnize_text(raw_text, self.state.settings)
        self.screens["scroll"].last_signature = None

    def set_realtime_rows(self, protocol: str, rows: List[str]):
        self.state.protocol = protocol
        self.state.raw_instruction_text = ""
        self.state.raw_rows = rows[:]
        self.state.scroll_text = realtime_columnize(rows)
        self.screens["scroll"].last_signature = None

    def run(self):
        running = True
        while running:
            dt = self.clock.tick(FPS) / 1000.0
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                    if self.state.screen == "main":
                        running = False
                    else:
                        self.switch("main")
                        continue
                self.screens[self.state.screen].handle_event(event)
            self.screens[self.state.screen].update(dt)
            self.screens[self.state.screen].draw(self.surface)
            pygame.display.flip()
        pygame.quit()
        sys.exit(0)

# ============================================================
# Drawing helpers
# ============================================================


def draw_retro_frame(surface):
    pygame.draw.rect(surface, CYAN, (12, 12, MENU_WIDTH - 24, MENU_HEIGHT - 24), 4, border_radius=12)
    pygame.draw.rect(surface, PURPLE, (24, 24, MENU_WIDTH - 48, MENU_HEIGHT - 48), 2, border_radius=10)
    for x in [42, MENU_WIDTH - 82]:
        for y in [42, MENU_HEIGHT - 82]:
            pygame.draw.rect(surface, YELLOW, (x, y, 12, 12))
            pygame.draw.rect(surface, GREEN, (x + 18, y, 12, 12))
            pygame.draw.rect(surface, RED, (x + 36, y, 12, 12))


if __name__ == "__main__":
    App().run()
