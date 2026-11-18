import tkinter as tk
from tkinter import font
import posix_ipc
import mmap
import struct
import time

# --- Configuration & Physics Parameters ---
TUBE_HEIGHT_CM = 50.0   # Total height of the tube in cm
CANVAS_HEIGHT = 500     # Height of the canvas in pixels (10 pixels = 1 cm)
CANVAS_WIDTH = 400
BALL_RADIUS = 15        # in pixels (1.5 cm radius)

# Physics model variables
ball_height_cm = 10.0   # Initial height in cm
ball_velocity = 0.0     # cm/s

SHM_NAME = "/ball_control"
SHM_SIZE = 12           # 3 floats * 4 bytes each

class BallTubeSimulator:
    def __init__(self, root):
        self.root = root
        self.root.title("Ball and Tube OS Project Simulator")
        self.root.geometry("600x550")
        self.root.config(bg="#f0f2f5")

        # --- Setup POSIX Shared Memory ---
        try:
            # Create or open POSIX shared memory block
            self.shm = posix_ipc.SharedMemory(SHM_NAME, posix_ipc.O_CREAT | posix_ipc.O_RDWR, size=SHM_SIZE)
            self.mmap_file = mmap.mmap(self.shm.fd, SHM_SIZE)
            # Close file descriptor as mmap holds it
            os_close_fd = self.shm.fd
            # Note: posix_ipc keeps fd reference safe via mmap
        except Exception as e:
            print(f"Error initializing shared memory: {e}")
            print("Make sure posix_ipc is installed: pip install posix_ipc")
            exit(1)

        # Initialize shared memory values: [height (10.0), speed (0.0), reference (25.0)]
        self.write_shm(10.0, 0.0, 25.0)

        # --- UI Layout ---
        title_font = font.Font(family="Helvetica", size=14, weight="bold")
        label_font = font.Font(family="Helvetica", size=11)

        header = tk.Label(root, text="OS Course: Ball & Tube Level Control Simulator", font=title_font, bg="#f0f2f5", fg="#333")
        header.pack(pady=10)

        # Main Frame containing Canvas and Control Panel
        main_frame = tk.Frame(root, bg="#f0f2f5")
        main_frame.pack(fill=tk.BOTH, expand=True, padx=20)

        # Canvas for Drawing Tube, Ball, Reference line, and Fan
        self.canvas = tk.Canvas(main_frame, width=CANVAS_WIDTH, height=CANVAS_HEIGHT, bg="white", highlightthickness=1, highlightbackground="#ccc")
        self.canvas.pack(side=tk.LEFT, padx=10)

        # Right Info Panel
        info_frame = tk.Frame(main_frame, bg="#f0f2f5", padx=10)
        info_frame.pack(side=tk.RIGHT, fill=tk.Y, anchor="n")

        tk.Label(info_frame, text="Telemetry & Stats", font=title_font, bg="#f0f2f5", fg="#111").pack(anchor="w", pady=(0, 10))

        self.lbl_height = tk.Label(info_frame, text="Height: 0.00 cm", font=label_font, bg="#f0f2f5")
        self.lbl_height.pack(anchor="w", pady=5)

        self.lbl_ref = tk.Label(info_frame, text="Reference: 0.00 cm", font=label_font, bg="#f0f2f5")
        self.lbl_ref.pack(anchor="w", pady=5)

        self.lbl_speed = tk.Label(info_frame, text="Fan Speed (V): 0.00", font=label_font, bg="#f0f2f5")
        self.lbl_speed.pack(anchor="w", pady=5)

        tk.Label(info_frame, text="\nShared Memory:", font=title_font, bg="#f0f2f5", fg="#111").pack(anchor="w", pady=(15, 5))
        tk.Label(info_frame, text="Path: /dev/shm/ball_control", font=("Courier", 9), bg="#f0f2f5", fg="#555").pack(anchor="w")
        tk.Label(info_frame, text="Variables (3 floats):\n[Height, Speed, Ref]", font=label_font, bg="#f0f2f5", justify=tk.LEFT).pack(anchor="w", pady=5)

        # Start simulation update loop (~50 fps)
        self.prev_time = time.time()
        self.update_simulation()

    def cm_to_px(self, cm):
        """Converts cm (0 at bottom, 50 at top) to canvas pixel coordinates (Y-inverted)."""
        # Tube is drawn from Y = 20 to Y = 470 (450 pixels span for 50 cm -> 9 px/cm)
        scale = 450.0 / TUBE_HEIGHT_CM
        return CANVAS_HEIGHT - 30 - (cm * scale)

    def read_shm(self):
        """Reads the 3 floats from shared memory: [height, fan_speed, reference_height]."""
        try:
            self.mmap_file.seek(0)
            data = self.mmap_file.read(SHM_SIZE)
            if len(data) == SHM_SIZE:
                return struct.unpack('3f', data)
        except Exception as e:
            print(f"Read SHM error: {e}")
        return 0.0, 0.0, 25.0

    def write_shm(self, height, speed, ref):
        """Writes the current height back to shared memory (keeping speed & ref intact)."""
        try:
            self.mmap_file.seek(0)
            data = struct.pack('3f', height, speed, ref)
            self.mmap_file.write(data)
        except Exception as e:
            print(f"Write SHM error: {e}")

    def update_simulation(self):
        global ball_height_cm, ball_velocity

        now = time.time()
        dt = now - self.prev_time
        if dt > 0.1: dt = 0.1 # Clamp delta time
        self.prev_time = now

        # Read control inputs from student's control process via shared memory
        curr_h, fan_v, ref_h = self.read_shm()

        # Clamp fan control variable V between 0.0 and 1.0
        fan_v = max(0.0, min(1.0, fan_v))
        ref_h = max(0.0, min(TUBE_HEIGHT_CM, ref_h))

        # --- Simple Dynamic Model ---
        # Upward force proportional to fan speed squared, downward gravity acceleration
        # Gravity pull = ~9.8 cm/s equivalent scaling, Fan lift max = ~15.0 cm/s^2
        gravity = -25.0
        lift = fan_v * 35.0
        
        acceleration = lift + gravity
        
        # Update velocity & position with damping (air resistance)
        ball_velocity += acceleration * dt
        ball_velocity *= 0.92  # Damping factor
        
        ball_height_cm += ball_velocity * dt

        # Tube boundaries clamping (0 cm to 50 cm)
        if ball_height_cm < 1.5:
            ball_height_cm = 1.5
            ball_velocity = 0.0
        elif ball_height_cm > TUBE_HEIGHT_CM - 1.5:
            ball_height_cm = TUBE_HEIGHT_CM - 1.5
            ball_velocity = 0.0

        # Write updated height back to shared memory for the student's controller
        self.write_shm(ball_height_cm, fan_v, ref_h)

        # --- Redraw Graphics ---
        self.canvas.delete("all")

        # Draw outer cabinet frame
        self.canvas.create_rectangle(120, 20, 280, 480, outline="#333", width=2, fill="#fafafa")
        
        # Draw inner transparent tube (X: 150 to 250 -> width 100px)
        self.canvas.create_rectangle(150, 20, 250, 470, outline="#999", width=2, fill="white")

        # Draw tick marks and cm height labels on the side of the tube
        for cm in range(0, int(TUBE_HEIGHT_CM) + 1, 5):
            y_px = self.cm_to_px(cm)
            self.canvas.create_line(242, y_px, 250, y_px, fill="#666")
            if cm % 10 == 0:
                self.canvas.create_text(265, y_px, text=f"{cm}cm", font=("Arial", 8), fill="#555")

        # Draw Fan at the bottom of the tube (Y: 470 to 490)
        fan_color = f"#{int(150 + 105*fan_v):02x}#{int(50 + 50*(1-fan_v)):02x}#{int(50 + 50*(1-fan_v)):02x}"
        self.canvas.create_rectangle(150, 470, 250, 495, fill="#2b2d42", outline="#111")
        self.canvas.create_text(200, 482, text=f"FAN (V={fan_v:.2f})", fill="white", font=("Arial", 9, "bold"))

        # Draw Reference Line (cyan horizontal line across the tube)
        ref_y = self.cm_to_px(ref_h)
        self.canvas.create_line(135, ref_y, 265, ref_y, fill="#00bcd4", width=3, dash=(4, 2))
        self.canvas.create_text(200, ref_y - 10, text=f"Ref: {ref_h:.1f} cm", fill="#00838f", font=("Arial", 9, "bold"))

        # Draw Ball (Red circle inside the tube)
        ball_y = self.cm_to_px(ball_height_cm)
        ball_x = 200 # Center of tube (150 + 250) / 2
        r = BALL_RADIUS
        self.canvas.create_oval(ball_x - r, ball_y - r, ball_x + r, ball_y + r, fill="#ff5252", outline="#b71c1c", width=2)

        # Update telemetry labels
        self.lbl_height.config(text=f"Height: {ball_height_cm:.2f} cm")
        self.lbl_ref.config(text=f"Reference: {ref_h:.2f} cm")
        self.lbl_speed.config(text=f"Fan Speed (V): {fan_v:.2f}")

        # Loop again
        self.root.after(20, self.update_simulation)

    def __del__(self):
        try:
            self.mmap_file.close()
            self.shm.close()
        except:
            pass

if __name__ == "__main__":
    root = tk.Tk()
    app = BallTubeSimulator(root)
    root.mainloop()