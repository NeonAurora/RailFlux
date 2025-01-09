# canvas.py
import pygame
import time
from train import Train
from track import Track
from firebase_admin import db
from settings import (
    GRID_ROWS, GRID_COLUMNS, CELL_SIZE,
    TRAIN_COLOR, TRAIN2_COLOR, SPEED,
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

        # Keep track of previously occupied segments for each train
        self.previous_segments = {
            'train1': None,
            'train2': None
        }

        # Firebase reference for starter signals
        self.signals_ref = db.reference('/signals/starters')
        self.signals_data = []  # We'll store the list from Firebase here
        self.last_signal_fetch_time = 0

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
    # MAIN DRAW
    def draw(self, screen):
        # Draw canvas background
        canvas_rect = pygame.Rect(0, self.header_height, self.width, self.height)
        pygame.draw.rect(screen, self.bg_color, canvas_rect)

        # Define the canvas area and draw grid, tracks, and trains
        canvas_area = screen.subsurface(canvas_rect)
        self.draw_grid(canvas_area)
        self.track.draw_tracks(canvas_area)
        self.train1.draw(canvas_area)
        self.train2.draw(canvas_area)

        # Draw the signals from Firebase (no more hard-coded calls)
        self.draw_fetched_signals(canvas_area)

    # --------------------------------------------------------------------------
    # UPDATE
    def update(self, screen):
        # 1) Move trains
        self.train1.move()
        self.train2.move()

        # 2) Update train segment occupation
        self.update_train_segment('train1')
        self.update_train_segment('train2')

        # 3) Fetch signals from Firebase (updates self.signals_data)
        self.fetch_signals_data()

        # 4) Draw everything
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
