# canvas.py
import pygame
import time
from train import Train
from track import Track
from firebase_admin import db
from settings import (
    GRID_ROWS, GRID_COLUMNS, CELL_SIZE,
    TRAIN_COLOR, TRAIN2_COLOR, TRAIN3_COLOR, LC_GATE_COLOR, LC_GATE_THICKNESS,
    LC_GATE_CIRCLE_RADIUS, SPEED,
    GRID_BORDER_COLOR, GRID_BORDER_THICKNESS,
    SECOND_PRIORITY_COLOR, TRAIN_TRANSPARENT
)

class Canvas:
    def __init__(self, width, height, bg_color, header_height):
        self.width = width
        self.height = height
        self.bg_color = bg_color
        self.header_height = header_height

        # Create a Track instance to draw multiple tracks
        self.track = Track(GRID_ROWS, GRID_COLUMNS, CELL_SIZE)

        # Initialize two trains, each assigned to a separate track with unique IDs
        self.train1_color = (*TRAIN_COLOR[:3], 0) if TRAIN_TRANSPARENT else TRAIN_COLOR
        self.train2_color = (*TRAIN2_COLOR[:3], 0) if TRAIN_TRANSPARENT else TRAIN2_COLOR
        self.train3_color = (*TRAIN3_COLOR[:3], 0) if TRAIN_TRANSPARENT else TRAIN3_COLOR

        self.train1 = Train(
            CELL_SIZE * 1.5, CELL_SIZE * 1.5,
            self.train1_color, SPEED,
            self.track.get_track_waypoints('T1'),
            train_id='train1'
        )

        self.train2 = Train(
            CELL_SIZE * 1.5, CELL_SIZE * 1.5,
            self.train2_color, SPEED,
            self.track.get_track_waypoints('T2', reverse=True),
            train_id='train2'
        )

        self.train3 = Train(
            CELL_SIZE * 1.5, CELL_SIZE * 1.5,
            self.train3_color, SPEED,
            self.track.get_track_waypoints('T3'),
            train_id='train3'
        )

        # Keep track of previously occupied segments for each train
        self.previous_segments = {
            'train1': None,
            'train2': None,
            'train3': None
        }

        # Firebase reference for starter signals
        self.signals_ref = db.reference('/signals/starters')
        self.signals_data = []  # We'll store the list from Firebase here
        self.last_signal_fetch_time = 0

        # Firebase reference for LC gates
        self.lc_gates_ref = db.reference('/signals/lc_gates')
        self.lc_gate_states = {'lc_gate1': False, 'lc_gate2': False}  # Default states
        self.last_lc_gate_fetch_time = 0

    # --------------------------------------------------------------------------
    # Fetch signals from Firebase (LIST format) and store in self.signals_data
    def fetch_signals_data(self):
        current_time = time.time()
        # Fetch once per second to avoid console spam or heavy DB usage
        if current_time - self.last_signal_fetch_time > 1.0:
            data = self.signals_ref.get()
            if data:
                print("Fetched starter signals from Firebase:", data)
                self.signals_data = data  # This is a LIST in your case
            else:
                print("No starter signals found.")
                self.signals_data = []
            self.last_signal_fetch_time = current_time

    # --------------------------------------------------------------------------
    # Fetch LC gate states from Firebase
    def fetch_lc_gate_states(self):
        current_time = time.time()
        # Fetch once per second to avoid console spam or heavy DB usage
        if current_time - self.last_lc_gate_fetch_time > 1.0:
            lc_gate_data = self.lc_gates_ref.get()
            if lc_gate_data:
                # Update the stored states with Firebase data
                self.lc_gate_states['lc_gate1'] = lc_gate_data.get('lc_gate1', False)
                self.lc_gate_states['lc_gate2'] = lc_gate_data.get('lc_gate2', False)
                print("Fetched LC gate states from Firebase:", self.lc_gate_states)
            else:
                print("No LC gate states found, using defaults.")
                self.lc_gate_states = {'lc_gate1': False, 'lc_gate2': False}
            self.last_lc_gate_fetch_time = current_time

    def grid_to_pixel(self, row, col):
        """
        Convert grid coordinates to pixel coordinates.
        IMPORTANT: Remember the coordinate system:
        - row corresponds to y-axis (vertical position)
        - col corresponds to x-axis (horizontal position)
        - x = col * CELL_SIZE
        - y = row * CELL_SIZE
        """
        x = col * CELL_SIZE
        y = row * CELL_SIZE
        return x, y

    # --------------------------------------------------------------------------
    # DRAW the signals that we fetched from Firebase
    def draw_fetched_signals(self, surface):
        # Define color mapping for statuses
        status_colors = {
            0: (0, 255, 0),     # Green
            1: (255, 0, 0),     # Red
            2: (255, 255, 0)    # Yellow
        }
        radius_in_cells = 1  # circle radius in terms of grid cells

        # self.signals_data is a list like:
        # [None, {'row': 68, 'col': 90, 'status': 0}, {'row': 58, 'col': 105, 'status': 1}, ... ]
        for entry in self.signals_data:
            # Skip any None entries
            if not entry:
                continue

            row = entry.get('row')
            col = entry.get('col')
            status = entry.get('status', 0)  # default to 0 (green) if missing

            # Safety check
            if row is None or col is None:
                continue

            color = status_colors.get(status, (128, 128, 128))  # fallback gray
            self.draw_signal_circle(surface, row, col, radius_in_cells, color)

    # --------------------------------------------------------------------------
    # GRID DRAWING
    def draw_grid(self, canvas):
        temp_surface = pygame.Surface((self.width, self.height), pygame.SRCALPHA)

        # Draw horizontal and vertical grid lines
        for row in range(GRID_ROWS + 1):
            y = row * CELL_SIZE
            color = SECOND_PRIORITY_COLOR if row % 10 == 0 else GRID_BORDER_COLOR
            pygame.draw.line(temp_surface, color, (0, y), (self.width, y), GRID_BORDER_THICKNESS)

        for col in range(GRID_COLUMNS + 1):
            x = col * CELL_SIZE
            color = SECOND_PRIORITY_COLOR if col % 10 == 0 else GRID_BORDER_COLOR
            pygame.draw.line(temp_surface, color, (x, 0), (x, self.height), GRID_BORDER_THICKNESS)

        canvas.blit(temp_surface, (0, 0))

    # --------------------------------------------------------------------------
    def draw_lc_gate_lines(self, surface, line_coords, color, thickness):
        """
        Draw multiple lines for LC gates.
        """
        for start_coords, end_coords in line_coords:
            start_row, start_col = start_coords
            end_row, end_col = end_coords
            start_x, start_y = self.grid_to_pixel(start_row, start_col)
            end_x, end_y = self.grid_to_pixel(end_row, end_col)
            pygame.draw.line(surface, color, (start_x, start_y), (end_x, end_y), thickness)

    def draw_lc_gate_circles(self, surface, circle_coords, color, radius):
        """
        Draw multiple circles for LC gates.
        """
        for row, col in circle_coords:
            center_x, center_y = self.grid_to_pixel(row, col)
            pygame.draw.circle(surface, color, (center_x, center_y), radius)

    # --------------------------------------------------------------------------
    def draw_lc_gate_status_indicators(self, surface, gate_number, gate_state):
        """
        Draw status indicators (red/green lines) for a specific LC gate.
        Red lines = gate activated/closed
        Green lines = gate deactivated/open

        Args:
            surface: pygame surface to draw on
            gate_number: 1 for first LC gate, 2 for second LC gate
            gate_state: True for activated/closed, False for deactivated/open
        """
        # Define coordinates for each gate
        gate_coordinates = {
            1: {  # First LC gate
                'red_lines': [
                    ((50, 35), (50, 45)),  # Vertical line from (50,35) to (50,45)
                    ((80, 45), (80, 35))  # Vertical line from (80,45) to (80,35)
                ],
                'green_lines': [
                    ((50, 35), (45, 40)),  # Diagonal line from (50,35) to (45,40)
                    ((80, 45), (75, 40))  # Diagonal line from (80,45) to (75,40)
                ]
            },
            2: {  # Second LC gate
                'red_lines': [
                    ((50, 210), (50, 220)),  # Vertical line from (50,210) to (50,220)
                    ((80, 220), (80, 210))  # Vertical line from (80,220) to (80,210)
                ],
                'green_lines': [
                    ((50, 210), (45, 215)),  # Diagonal line from (50,210) to (45,215)
                    ((80, 220), (75, 215))  # Diagonal line from (80,220) to (75,215)
                ]
            }
        }

        # Get coordinates for the specified gate
        if gate_number not in gate_coordinates:
            return  # Invalid gate number

        coords = gate_coordinates[gate_number]

        if gate_state:  # Activated/Closed - Red lines
            red_color = (255, 0, 0)
            red_thickness = 3
            self.draw_lc_gate_lines(surface, coords['red_lines'], red_color, red_thickness)
        else:  # Deactivated/Open - Green lines
            green_color = (0, 255, 0)
            green_thickness = 3
            self.draw_lc_gate_lines(surface, coords['green_lines'], green_color, green_thickness)

    def draw_lc_gates(self, surface):
        """
        Draw Level Crossing (LC) gates.
        Includes white base lines, circles, and status indicators.

        COORDINATE SYSTEM REMINDER:
        - All coordinates are in (row, col) format
        - row = y-axis position, col = x-axis position
        - Conversion: x = col * CELL_SIZE, y = row * CELL_SIZE
        """
        # LC gate properties
        lc_gate_color = LC_GATE_COLOR
        lc_gate_thickness = LC_GATE_THICKNESS
        circle_radius = LC_GATE_CIRCLE_RADIUS

        # Get LC gate states from Firebase (fetched in update method)
        lc_gate1 = self.lc_gate_states['lc_gate1']
        lc_gate2 = self.lc_gate_states['lc_gate2']

        # Define coordinates for LC gate base lines
        lc_gate_lines = [
            # First LC gate (remember: row, col format)
            ((40, 35), (90, 35)),  # Horizontal line at row 40, from col 35 to 90
            ((40, 45), (90, 45)),  # Horizontal line at row 45, from col 35 to 90
            # Second LC gate
            ((40, 210), (90, 210)),  # Horizontal line at row 210, from col 35 to 90
            ((40, 220), (90, 220))  # Horizontal line at row 220, from col 35 to 90
        ]

        # Define coordinates for LC gate circles
        circle_positions = [
            (50, 35),  # First circle (row 50, col 35)
            (80, 45),  # Second circle (row 80, col 45)
            (50, 210),  # Third circle (row 50, col 210)
            (80, 220)  # Fourth circle (row 80, col 220)
        ]

        # Draw LC gate base lines
        self.draw_lc_gate_lines(surface, lc_gate_lines, lc_gate_color, lc_gate_thickness)

        # Draw LC gate circles
        self.draw_lc_gate_circles(surface, circle_positions, lc_gate_color, circle_radius)

        # Draw status indicators for both LC gates
        self.draw_lc_gate_status_indicators(surface, 1, lc_gate1)  # First LC gate
        self.draw_lc_gate_status_indicators(surface, 2, lc_gate2)  # Second LC gate

    # --------------------------------------------------------------------------
    # MAIN DRAW
    def draw(self, screen):
        # Draw canvas background
        canvas_rect = pygame.Rect(0, self.header_height, self.width, self.height)
        pygame.draw.rect(screen, self.bg_color, canvas_rect)

        # Define the canvas area and draw grid, tracks, and trains
        canvas_area = screen.subsurface(canvas_rect)
        self.draw_grid(canvas_area)
        self.draw_lc_gates(canvas_area)  # Draw LC gates
        self.track.draw_tracks(canvas_area)
        self.train1.draw(canvas_area)
        self.train2.draw(canvas_area)
        self.train3.draw(canvas_area)

        # Draw the signals from Firebase (no more hard-coded calls)
        self.draw_fetched_signals(canvas_area)

    # --------------------------------------------------------------------------
    # UPDATE
    def update(self, screen):
        # 1) Move trains
        self.train1.move()
        self.train2.move()
        self.train3.move()

        # 2) Update train segment occupation
        self.update_train_segment('train1')
        self.update_train_segment('train2')
        self.update_train_segment('train3')

        # 3) Fetch signals from Firebase (updates self.signals_data)
        self.fetch_signals_data()

        # 4) Fetch LC gate states from Firebase
        self.fetch_lc_gate_states()

        # 5) Draw everything
        self.draw(screen)

    # --------------------------------------------------------------------------
    # TRAIN SEGMENT UPDATES
    def update_train_segment(self, train_name):
        train_ref = db.reference(f'/trains/{train_name}')
        train_data = train_ref.get()

        if not train_data:
            return

        current_track = train_data.get('current_track')
        current_segment = train_data.get('current_segment')
        self.previous_segments[train_name] = (current_track, current_segment)

    # --------------------------------------------------------------------------
    # SIGNAL DRAWING HELPER
    def draw_signal_circle(self, surface, row, col, radius_in_cells, color):
        center_x = int(col * CELL_SIZE)
        center_y = int(row * CELL_SIZE)
        radius_pixels = int(radius_in_cells * CELL_SIZE)
        pygame.draw.circle(surface, color, (center_x, center_y), radius_pixels)