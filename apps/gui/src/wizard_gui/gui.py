from __future__ import annotations

import queue
import tkinter as tk
import time
from datetime import datetime
from tkinter import ttk
from tkinter.scrolledtext import ScrolledText
from typing import Any

from perseus_shared.protocol import (
    VALID_STATES,
    build_flower_raw_command,
    build_flower_tilt_command,
    build_set_state_command,
    build_vibration_command,
    build_vibration_level_command,
)
from .device_registry import DeviceRegistry
from .server import TCPServer


HEARTBEAT_ACTIVE_SECONDS = 12.0
MAX_LOG_LINES = 500
MAX_EVENTS_PER_POLL = 200
MAX_LOGS_PER_POLL = 100


class WizardGUI:
    def __init__(self, root: tk.Tk, registry: DeviceRegistry, server: TCPServer) -> None:
        self.root = root
        self.registry = registry
        self.server = server
        self.server.log_callback = self.log

        self.selected_device_id: str | None = None
        self.selected_device_kind: str | None = None
        self.abstract_vibration_on = False
        self.abstract_vibration_level_var = tk.DoubleVar(value=0.65)

        self.flower_run_var = tk.BooleanVar(value=False)
        self.flower_speed_var = tk.DoubleVar(value=0.5)
        self.flower_amplitude_var = tk.DoubleVar(value=0.5)
        self.flower_tilt_var = tk.DoubleVar(value=0.5)
        self.reset_light_canvas: tk.Canvas | None = None
        self.reset_light_oval: int | None = None
        self.controls_canvas: tk.Canvas | None = None
        self.controls_window_id: int | None = None
        self._log_queue: queue.Queue[str] = queue.Queue()
        self._log_line_count = 0
        self._variable_trace_tokens: list[tuple[tk.Variable, str]] = []
        self._last_periodic_refresh = 0.0

        self.root.title("Wizard-of-Oz Controller")
        self.root.geometry("1100x700")

        self.heartbeat_active_image = self._make_status_dot("#18a558")
        self.heartbeat_inactive_image = self._make_status_dot("#b8b8b8")

        self._build_layout()
        self.refresh_device_list()
        self.poll_server_events()

    def _make_status_dot(self, color: str) -> tk.PhotoImage:
        image = tk.PhotoImage(width=14, height=14)
        center = 6.5
        radius = 5.5
        for y in range(14):
            for x in range(14):
                if (x - center) ** 2 + (y - center) ** 2 <= radius**2:
                    image.put(color, (x, y))
        return image

    def _build_layout(self) -> None:
        self.root.rowconfigure(0, weight=1)
        self.root.rowconfigure(1, weight=0)
        self.root.columnconfigure(0, weight=1)

        main_frame = ttk.Frame(self.root, padding=8)
        main_frame.grid(row=0, column=0, sticky="nsew")
        main_frame.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=2)

        self._build_left_panel(main_frame)
        self._build_right_panel(main_frame)
        self._build_log_panel()

    def _build_left_panel(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(parent, text="Connected Devices", padding=8)
        frame.grid(row=0, column=0, sticky="nsew", padx=(0, 8))
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)

        columns = ("device_id", "device_kind", "status", "last_event")
        self.device_tree = ttk.Treeview(frame, columns=columns, show="tree headings", height=16)
        self.device_tree.heading("#0", text="heartbeat")
        self.device_tree.heading("device_id", text="device_id")
        self.device_tree.heading("device_kind", text="device_kind")
        self.device_tree.heading("status", text="status")
        self.device_tree.heading("last_event", text="last_event")
        self.device_tree.column("#0", width=84, minwidth=84, stretch=False, anchor="center")
        self.device_tree.column("device_id", width=180)
        self.device_tree.column("device_kind", width=140)
        self.device_tree.column("status", width=80)
        self.device_tree.column("last_event", width=120)
        self.device_tree.grid(row=0, column=0, sticky="nsew")
        self.device_tree.bind("<<TreeviewSelect>>", self.on_device_selected)

        scrollbar = ttk.Scrollbar(frame, orient="vertical", command=self.device_tree.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.device_tree.configure(yscrollcommand=scrollbar.set)

        refresh_button = ttk.Button(frame, text="Refresh", command=self.refresh_device_list)
        refresh_button.grid(row=1, column=0, sticky="ew", pady=(8, 0))

    def _build_right_panel(self, parent: ttk.Frame) -> None:
        self.control_container = ttk.LabelFrame(parent, text="Controls", padding=8)
        self.control_container.grid(row=0, column=1, sticky="nsew")
        self.control_container.rowconfigure(1, weight=1)
        self.control_container.columnconfigure(0, weight=1)

        self.selected_label = ttk.Label(self.control_container, text="No device selected")
        self.selected_label.grid(row=0, column=0, sticky="w", pady=(0, 8))

        self.controls_canvas = tk.Canvas(
            self.control_container,
            borderwidth=0,
            highlightthickness=0,
        )
        controls_scrollbar = ttk.Scrollbar(
            self.control_container,
            orient="vertical",
            command=self.controls_canvas.yview,
        )
        self.controls_canvas.configure(yscrollcommand=controls_scrollbar.set)
        self.controls_canvas.grid(row=1, column=0, sticky="nsew")
        controls_scrollbar.grid(row=1, column=1, sticky="ns")

        self.dynamic_controls = ttk.Frame(self.controls_canvas)
        self.dynamic_controls.columnconfigure(0, weight=1)
        self.controls_window_id = self.controls_canvas.create_window(
            (0, 0),
            window=self.dynamic_controls,
            anchor="nw",
        )
        self.dynamic_controls.bind("<Configure>", self._update_controls_scroll_region)
        self.controls_canvas.bind("<Configure>", self._resize_controls_window)
        self.controls_canvas.bind("<MouseWheel>", self._on_controls_mousewheel)
        self.dynamic_controls.bind("<MouseWheel>", self._on_controls_mousewheel)

        self.show_no_selection()

    def _update_controls_scroll_region(self, _event: tk.Event) -> None:
        if self.controls_canvas is None:
            return
        self.controls_canvas.configure(scrollregion=self.controls_canvas.bbox("all"))
        self._bind_controls_mousewheel_handlers(self.dynamic_controls)

    def _resize_controls_window(self, event: tk.Event) -> None:
        if self.controls_canvas is None or self.controls_window_id is None:
            return
        self.controls_canvas.itemconfigure(self.controls_window_id, width=event.width)

    def _bind_controls_mousewheel_handlers(self, widget: tk.Widget) -> None:
        widget.bind("<MouseWheel>", self._on_controls_mousewheel)
        for child in widget.winfo_children():
            self._bind_controls_mousewheel_handlers(child)

    def _on_controls_mousewheel(self, event: tk.Event) -> str:
        if self.controls_canvas is None:
            return "break"
        self.controls_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")
        return "break"

    def _build_log_panel(self) -> None:
        frame = ttk.LabelFrame(self.root, text="Debug Log", padding=8)
        frame.grid(row=1, column=0, sticky="nsew", padx=8, pady=(0, 8))
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)

        self.log_text = ScrolledText(frame, height=14, state="disabled")
        self.log_text.grid(row=0, column=0, sticky="nsew")

    def log(self, message: str) -> None:
        self._log_queue.put(message)

    def _append_log_batch(self, messages: list[str]) -> None:
        if not messages:
            return

        self.log_text.configure(state="normal")
        self.log_text.insert("end", "\n".join(messages) + "\n")
        self._log_line_count += len(messages)
        if self._log_line_count > MAX_LOG_LINES:
            lines_to_remove = self._log_line_count - MAX_LOG_LINES
            self.log_text.delete("1.0", f"{lines_to_remove + 1}.0")
            self._log_line_count = MAX_LOG_LINES
        self.log_text.see("end")
        self.log_text.configure(state="disabled")

    def poll_server_events(self) -> None:
        refresh_needed = False
        reset_light_needed = False

        for _ in range(MAX_EVENTS_PER_POLL):
            try:
                event = self.server.gui_queue.get_nowait()
            except queue.Empty:
                break

            if event.get("kind") == "device_list_changed":
                refresh_needed = True
                reset_light_needed = True

        log_messages: list[str] = []
        for _ in range(MAX_LOGS_PER_POLL):
            try:
                log_messages.append(self._log_queue.get_nowait())
            except queue.Empty:
                break

        self._append_log_batch(log_messages)

        if refresh_needed:
            self.refresh_device_list()
        if reset_light_needed:
            self.update_reset_light()

        now = time.monotonic()
        if now - self._last_periodic_refresh >= 1.0:
            self.refresh_device_list()
            self.update_reset_light()
            self._last_periodic_refresh = now

        self.root.after(200, self.poll_server_events)

    def refresh_device_list(self) -> None:
        selected = self.selected_device_id

        for item_id in self.device_tree.get_children():
            self.device_tree.delete(item_id)

        for device in self.registry.list_devices():
            status = "online" if device["connected"] else "offline"
            heartbeat_image = (
                self.heartbeat_active_image
                if self._heartbeat_active(device)
                else self.heartbeat_inactive_image
            )
            self.device_tree.insert(
                "",
                "end",
                iid=device["device_id"],
                image=heartbeat_image,
                values=(
                    device["device_id"],
                    device["device_kind"],
                    status,
                    device["last_event"],
                ),
            )

        if selected and self.device_tree.exists(selected):
            self.device_tree.selection_set(selected)
            self.device_tree.focus(selected)
        elif selected and not self.device_tree.exists(selected):
            self.selected_device_id = None
            self.selected_device_kind = None
            self.show_no_selection()

    def _heartbeat_active(self, device: dict[str, object]) -> bool:
        if not device.get("connected"):
            return False

        last_heartbeat = device.get("last_heartbeat")
        if not isinstance(last_heartbeat, str) or not last_heartbeat:
            return False

        try:
            age_seconds = (datetime.now() - datetime.fromisoformat(last_heartbeat)).total_seconds()
        except ValueError:
            return False

        return age_seconds <= HEARTBEAT_ACTIVE_SECONDS

    def on_device_selected(self, _event: object) -> None:
        selection = self.device_tree.selection()
        if not selection:
            self.selected_device_id = None
            self.selected_device_kind = None
            self.show_no_selection()
            return

        device_id = selection[0]
        device = self.registry.get(device_id)
        if device is None:
            self.selected_device_id = None
            self.selected_device_kind = None
            self.show_no_selection()
            return

        if (
            device_id == self.selected_device_id
            and device.device_kind == self.selected_device_kind
        ):
            self.selected_label.configure(
                text=f"Selected: {device.device_id} ({device.device_kind})"
            )
            self.update_reset_light()
            return

        self.selected_device_id = device_id
        self.selected_device_kind = device.device_kind
        self.selected_label.configure(
            text=f"Selected: {device.device_id} ({device.device_kind})"
        )
        self.show_controls_for_kind(device.device_kind)

    def clear_dynamic_controls(self) -> None:
        for variable, token in self._variable_trace_tokens:
            try:
                variable.trace_remove("write", token)
            except tk.TclError:
                pass
        self._variable_trace_tokens.clear()

        for child in self.dynamic_controls.winfo_children():
            child.destroy()
        self.reset_light_canvas = None
        self.reset_light_oval = None
        if self.controls_canvas is not None:
            self.controls_canvas.yview_moveto(0.0)

    def _add_variable_trace(self, variable: tk.Variable, callback: Any) -> None:
        token = variable.trace_add("write", callback)
        self._variable_trace_tokens.append((variable, token))

    def show_no_selection(self) -> None:
        self.clear_dynamic_controls()
        label = ttk.Label(self.dynamic_controls, text="Select a device from the list.")
        label.grid(row=0, column=0, sticky="w")
        self.selected_label.configure(text="No device selected")
        self.selected_device_kind = None

    def show_controls_for_kind(self, device_kind: str) -> None:
        self.clear_dynamic_controls()
        self._build_reset_requested_indicator(self.dynamic_controls, row=0)

        if device_kind == "anthro_sphere":
            self._build_state_buttons(self.dynamic_controls, row=1)
        elif device_kind == "abstract_sphere":
            self._build_state_buttons(self.dynamic_controls, row=1)
            self._build_abstract_vibration_controls(self.dynamic_controls, row=2)
        elif device_kind == "flower":
            self._build_state_buttons(self.dynamic_controls, row=1)
            self._build_flower_raw_controls(self.dynamic_controls, row=2)
            self._build_flower_tilt_controls(self.dynamic_controls, row=3)
        else:
            label = ttk.Label(self.dynamic_controls, text=f"No controls for {device_kind}")
            label.grid(row=1, column=0, sticky="w")

    def _build_reset_requested_indicator(self, parent: ttk.Frame, row: int) -> None:
        reset_frame = ttk.LabelFrame(parent, text="Device Feedback", padding=8)
        reset_frame.grid(row=row, column=0, sticky="ew", pady=(0, 8))

        self.reset_light_canvas = tk.Canvas(
            reset_frame,
            width=18,
            height=18,
            highlightthickness=0,
        )
        self.reset_light_canvas.grid(row=0, column=0, sticky="w")
        self.reset_light_oval = self.reset_light_canvas.create_oval(
            3,
            3,
            15,
            15,
            outline="#5f6368",
            fill="#d1d5db",
        )

        label = ttk.Label(reset_frame, text="Reset requested")
        label.grid(row=0, column=1, sticky="w", padx=(8, 0))
        self.update_reset_light()

    def update_reset_light(self) -> None:
        if self.reset_light_canvas is None or self.reset_light_oval is None:
            return
        if self.selected_device_id is None:
            reset_requested = False
        else:
            device = self.registry.get(self.selected_device_id)
            reset_requested = bool(device and device.reset_requested)

        fill = "#d92d20" if reset_requested else "#d1d5db"
        outline = "#b42318" if reset_requested else "#5f6368"
        self.reset_light_canvas.itemconfigure(
            self.reset_light_oval,
            fill=fill,
            outline=outline,
        )

    def _build_state_buttons(self, parent: ttk.Frame, row: int) -> None:
        state_frame = ttk.LabelFrame(parent, text="States", padding=8)
        state_frame.grid(row=row, column=0, sticky="ew")
        state_frame.columnconfigure(0, weight=1)
        state_frame.columnconfigure(1, weight=1)

        for index, state in enumerate(VALID_STATES):
            button = ttk.Button(
                state_frame,
                text=state,
                command=lambda value=state: self.send_state(value),
            )
            button.grid(row=index // 2, column=index % 2, sticky="ew", padx=4, pady=4)

    def _build_abstract_vibration_controls(self, parent: ttk.Frame, row: int) -> None:
        vibration_frame = ttk.LabelFrame(parent, text="Vibration", padding=8)
        vibration_frame.grid(row=row, column=0, sticky="ew", pady=(8, 0))
        vibration_frame.columnconfigure(0, weight=1)

        vibration_button = ttk.Button(
            vibration_frame,
            text="Hold to Vibrate",
        )
        vibration_button.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        vibration_button.bind("<ButtonPress-1>", self.start_vibration_hold)
        vibration_button.bind("<ButtonRelease-1>", self.stop_vibration_hold)
        vibration_button.bind("<Leave>", self.stop_vibration_hold)

        ttk.Label(vibration_frame, text="Vibration Level").grid(row=1, column=0, sticky="w")
        level_scale = ttk.Scale(
            vibration_frame,
            from_=0.0,
            to=1.0,
            variable=self.abstract_vibration_level_var,
            orient="horizontal",
        )
        level_scale.grid(row=2, column=0, sticky="ew")

        level_label = ttk.Label(
            vibration_frame,
            text=f"level={self.abstract_vibration_level_var.get():.2f}",
        )
        level_label.grid(row=3, column=0, sticky="w", pady=(4, 4))

        def update_level_label(*_args: object) -> None:
            if not level_label.winfo_exists():
                return
            level_label.configure(text=f"level={self.abstract_vibration_level_var.get():.2f}")

        self._add_variable_trace(self.abstract_vibration_level_var, update_level_label)

        send_level_button = ttk.Button(
            vibration_frame,
            text="Send Vibration Level",
            command=self.send_vibration_level,
        )
        send_level_button.grid(row=4, column=0, sticky="ew", pady=(4, 0))

    def _build_flower_raw_controls(self, parent: ttk.Frame, row: int) -> None:
        raw_frame = ttk.LabelFrame(parent, text="Raw Flower Rotation Controls", padding=8)
        raw_frame.grid(row=row, column=0, sticky="ew", pady=(8, 0))
        raw_frame.columnconfigure(1, weight=1)

        run_check = ttk.Checkbutton(raw_frame, text="Run", variable=self.flower_run_var)
        run_check.grid(row=0, column=0, sticky="w", pady=4)

        ttk.Label(raw_frame, text="Speed").grid(row=1, column=0, sticky="w")
        speed_scale = ttk.Scale(
            raw_frame,
            from_=0.0,
            to=1.0,
            variable=self.flower_speed_var,
            orient="horizontal",
        )
        speed_scale.grid(row=1, column=1, sticky="ew", padx=(8, 0))

        ttk.Label(raw_frame, text="Amplitude").grid(row=2, column=0, sticky="w")
        amplitude_scale = ttk.Scale(
            raw_frame,
            from_=0.0,
            to=1.0,
            variable=self.flower_amplitude_var,
            orient="horizontal",
        )
        amplitude_scale.grid(row=2, column=1, sticky="ew", padx=(8, 0))

        value_label = ttk.Label(
            raw_frame,
            text=self._format_flower_values(
                self.flower_speed_var.get(), self.flower_amplitude_var.get()
            ),
        )
        value_label.grid(row=3, column=0, columnspan=2, sticky="w", pady=(4, 4))

        def update_value_label(*_args: object) -> None:
            if not value_label.winfo_exists():
                return
            value_label.configure(
                text=self._format_flower_values(
                    self.flower_speed_var.get(), self.flower_amplitude_var.get()
                )
            )

        self._add_variable_trace(self.flower_speed_var, update_value_label)
        self._add_variable_trace(self.flower_amplitude_var, update_value_label)

        send_button = ttk.Button(
            raw_frame,
            text="Send Rotation Command",
            command=self.send_flower_raw,
        )
        send_button.grid(row=4, column=0, columnspan=2, sticky="ew", pady=(8, 0))

    def _format_flower_values(self, speed: float, amplitude: float) -> str:
        return f"speed={speed:.2f} amplitude={amplitude:.2f}"

    def _build_flower_tilt_controls(self, parent: ttk.Frame, row: int) -> None:
        tilt_frame = ttk.LabelFrame(parent, text="Flower Tilt Control", padding=8)
        tilt_frame.grid(row=row, column=0, sticky="ew", pady=(8, 0))
        tilt_frame.columnconfigure(1, weight=1)

        ttk.Label(tilt_frame, text="Tilt").grid(row=0, column=0, sticky="w")
        tilt_scale = ttk.Scale(
            tilt_frame,
            from_=0.0,
            to=1.0,
            variable=self.flower_tilt_var,
            orient="horizontal",
        )
        tilt_scale.grid(row=0, column=1, sticky="ew", padx=(8, 0))

        value_label = ttk.Label(
            tilt_frame,
            text=self._format_flower_tilt(self.flower_tilt_var.get()),
        )
        value_label.grid(row=1, column=0, columnspan=2, sticky="w", pady=(4, 4))

        def update_value_label(*_args: object) -> None:
            if not value_label.winfo_exists():
                return
            value_label.configure(text=self._format_flower_tilt(self.flower_tilt_var.get()))

        self._add_variable_trace(self.flower_tilt_var, update_value_label)

        send_button = ttk.Button(
            tilt_frame,
            text="Send Tilt Command",
            command=self.send_flower_tilt,
        )
        send_button.grid(row=2, column=0, columnspan=2, sticky="ew", pady=(8, 0))

    def _format_flower_tilt(self, tilt: float) -> str:
        return f"tilt={tilt:.2f}"

    def send_state(self, state: str) -> None:
        if self.selected_device_id is None:
            self.log("[gui] No device selected")
            return
        device = self.registry.get(self.selected_device_id)
        if device is None:
            self.log(f"[gui] Unknown device: {self.selected_device_id}")
            return
        self.send_operator_message(
            device.device_id,
            build_set_state_command(device.device_id, state),
        )

    def send_operator_message(self, device_id: str, message: dict[str, Any]) -> None:
        if self.registry.clear_reset_requested(device_id):
            self.update_reset_light()
        self.server.send_message(device_id, message)

    def start_vibration_hold(self, _event: object | None = None) -> None:
        if self.abstract_vibration_on:
            return
        self.abstract_vibration_on = True
        self.send_vibration(True)

    def stop_vibration_hold(self, _event: object | None = None) -> None:
        if not self.abstract_vibration_on:
            return
        self.abstract_vibration_on = False
        self.send_vibration(False)

    def send_vibration(self, enabled: bool) -> None:
        if self.selected_device_id is None:
            self.log("[gui] No device selected")
            return
        device = self.registry.get(self.selected_device_id)
        if device is None:
            self.log(f"[gui] Unknown device: {self.selected_device_id}")
            return
        self.send_operator_message(
            device.device_id,
            build_vibration_command(
                device.device_id,
                enabled,
            ),
        )

    def send_vibration_level(self) -> None:
        if self.selected_device_id is None:
            self.log("[gui] No device selected")
            return
        device = self.registry.get(self.selected_device_id)
        if device is None:
            self.log(f"[gui] Unknown device: {self.selected_device_id}")
            return
        self.send_operator_message(
            device.device_id,
            build_vibration_level_command(
                device.device_id,
                self.abstract_vibration_level_var.get(),
            ),
        )

    def send_flower_raw(self) -> None:
        if self.selected_device_id is None:
            self.log("[gui] No device selected")
            return
        device = self.registry.get(self.selected_device_id)
        if device is None:
            self.log(f"[gui] Unknown device: {self.selected_device_id}")
            return
        message = build_flower_raw_command(
            device_id=device.device_id,
            run=self.flower_run_var.get(),
            speed=self.flower_speed_var.get(),
            amplitude=self.flower_amplitude_var.get(),
        )
        self.send_operator_message(device.device_id, message)

    def send_flower_tilt(self) -> None:
        if self.selected_device_id is None:
            self.log("[gui] No device selected")
            return
        device = self.registry.get(self.selected_device_id)
        if device is None:
            self.log(f"[gui] Unknown device: {self.selected_device_id}")
            return
        message = build_flower_tilt_command(
            device_id=device.device_id,
            tilt=self.flower_tilt_var.get(),
        )
        self.send_operator_message(device.device_id, message)
