from __future__ import annotations

import queue
import tkinter as tk
import time
from datetime import datetime
from tkinter import ttk
from tkinter.scrolledtext import ScrolledText

from perseus_shared.protocol import (
    VALID_STATES,
    build_flower_raw_command,
    build_set_state_command,
    build_vibration_command,
    build_vibration_level_command,
)
from .device_registry import DeviceRegistry
from .server import TCPServer


HEARTBEAT_ACTIVE_SECONDS = 12.0


class WizardGUI:
    def __init__(self, root: tk.Tk, registry: DeviceRegistry, server: TCPServer) -> None:
        self.root = root
        self.registry = registry
        self.server = server
        self.server.log_callback = self.log

        self.selected_device_id: str | None = None
        self.abstract_vibration_on = False
        self.abstract_vibration_level_var = tk.DoubleVar(value=0.65)

        self.flower_run_var = tk.BooleanVar(value=False)
        self.flower_speed_var = tk.DoubleVar(value=0.5)
        self.flower_amplitude_var = tk.DoubleVar(value=0.5)
        self.reset_light_canvas: tk.Canvas | None = None
        self.reset_light_oval: int | None = None
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

        self.dynamic_controls = ttk.Frame(self.control_container)
        self.dynamic_controls.grid(row=1, column=0, sticky="nsew")

        self.show_no_selection()

    def _build_log_panel(self) -> None:
        frame = ttk.LabelFrame(self.root, text="Debug Log", padding=8)
        frame.grid(row=1, column=0, sticky="nsew", padx=8, pady=(0, 8))
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)

        self.log_text = ScrolledText(frame, height=14, state="disabled")
        self.log_text.grid(row=0, column=0, sticky="nsew")

    def log(self, message: str) -> None:
        self.root.after(0, self._append_log, message)

    def _append_log(self, message: str) -> None:
        self.log_text.configure(state="normal")
        self.log_text.insert("end", message + "\n")
        self.log_text.see("end")
        self.log_text.configure(state="disabled")

    def poll_server_events(self) -> None:
        while True:
            try:
                event = self.server.gui_queue.get_nowait()
            except queue.Empty:
                break

            if event.get("kind") == "device_list_changed":
                self.refresh_device_list()
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
            self.show_no_selection()
            return

        device_id = selection[0]
        self.selected_device_id = device_id
        device = self.registry.get(device_id)
        if device is None:
            self.show_no_selection()
            return

        self.selected_label.configure(
            text=f"Selected: {device.device_id} ({device.device_kind})"
        )
        self.show_controls_for_kind(device.device_kind)

    def clear_dynamic_controls(self) -> None:
        for child in self.dynamic_controls.winfo_children():
            child.destroy()
        self.reset_light_canvas = None
        self.reset_light_oval = None

    def show_no_selection(self) -> None:
        self.clear_dynamic_controls()
        label = ttk.Label(self.dynamic_controls, text="Select a device from the list.")
        label.grid(row=0, column=0, sticky="w")
        self.selected_label.configure(text="No device selected")

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
            level_label.configure(text=f"level={self.abstract_vibration_level_var.get():.2f}")

        self.abstract_vibration_level_var.trace_add("write", update_level_label)

        send_level_button = ttk.Button(
            vibration_frame,
            text="Send Vibration Level",
            command=self.send_vibration_level,
        )
        send_level_button.grid(row=4, column=0, sticky="ew", pady=(4, 0))

    def _build_flower_raw_controls(self, parent: ttk.Frame, row: int) -> None:
        raw_frame = ttk.LabelFrame(parent, text="Flower Raw", padding=8)
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
            value_label.configure(
                text=self._format_flower_values(
                    self.flower_speed_var.get(), self.flower_amplitude_var.get()
                )
            )

        self.flower_speed_var.trace_add("write", update_value_label)
        self.flower_amplitude_var.trace_add("write", update_value_label)

        send_button = ttk.Button(raw_frame, text="Send Raw Command", command=self.send_flower_raw)
        send_button.grid(row=4, column=0, columnspan=2, sticky="ew", pady=(8, 0))

    def _format_flower_values(self, speed: float, amplitude: float) -> str:
        return f"speed={speed:.2f} amplitude={amplitude:.2f}"

    def send_state(self, state: str) -> None:
        if self.selected_device_id is None:
            self.log("[gui] No device selected")
            return
        device = self.registry.get(self.selected_device_id)
        if device is None:
            self.log(f"[gui] Unknown device: {self.selected_device_id}")
            return
        self.server.send_message(
            self.selected_device_id,
            build_set_state_command(device.device_id, state),
        )

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
        self.server.send_message(
            self.selected_device_id,
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
        self.server.send_message(
            self.selected_device_id,
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
        self.server.send_message(self.selected_device_id, message)
