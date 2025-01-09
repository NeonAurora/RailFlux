# canvas.py
import pygame
from train import Train
from track import Track
from firebase_admin import db
from settings import (GRID_ROWS, GRID_COLUMNS, CELL_SIZE, TRAIN_COLOR, TRAIN2_COLOR, SPEED,
                      GRID_BORDER_COLOR, GRID_BORDER_THICKNESS, SECOND_PRIORITY_COLOR, TRAIN_TRANSPARENT)

class Canvas:
    def __init__(self, width, height, bg_color, header_height):
        self.width = width
        self.height = height
        self.bg_color = bg_color
        self.header_height = header_height

        # Create a Track instance to draw multiple tracks
        self.track = Track(GRID_ROWS, GRID_COLUMNS, CELL_SIZE)

        # Initialize two trains, each assigned to a separate track with unique train IDs
        # Use TRAIN_TRANSPARENT to determine the train color (if transparent, make alpha 0)
        self.train1_color = (*TRAIN_COLOR[:3], 0) if TRAIN_TRANSPARENT else TRAIN_COLOR
        self.train2_color = (*TRAIN2_COLOR[:3], 0) if TRAIN_TRANSPARENT else TRAIN2_COLOR

        # Initialize two trains with unique IDs
        self.train1 = Train(CELL_SIZE * 1.5, CELL_SIZE * 1.5, self.train1_color, SPEED, self.track.get_track_waypoints('T1'), train_id='train1')
        self.train2 = Train(CELL_SIZE * 1.5, CELL_SIZE * 1.5, self.train2_color, SPEED, self.track.get_track_waypoints('T2', reverse=True), train_id='train2')

        # Keep track of the previously occupied segments for each train
        self.previous_segments = {
            'train1': None,
            'train2': None
        }

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

        # 3) Draw multiple signals with various statuses
        #    Example: signal_id=1 => green(0), signal_id=2 => red(1), etc.
        self.handle_starter_signal(canvas_area, 1, 0)  # Signal #1, status=0 (green)
        self.handle_starter_signal(canvas_area, 2, 1)  # Signal #2, status=1 (red)
        self.handle_starter_signal(canvas_area, 3, 2)  # Signal #3, status=2 (yellow)
        self.handle_starter_signal(canvas_area, 4, 1)  # ...
        self.handle_starter_signal(canvas_area, 5, 0)  # ...

    def update(self, screen):
        # Update both trains
        self.train1.move()
        self.train2.move()

        # Update segment occupation for train 1
        self.update_train_segment('train1')

        # Update segment occupation for train 2
        self.update_train_segment('train2')

        # Redraw everything on the screen
        self.draw(screen)

    def update_train_segment(self, train_name):
        # Reference the train data from Firebase
        train_ref = db.reference(f'/trains/{train_name}')
        train_data = train_ref.get()

        if not train_data:
            return

        current_track = train_data.get('current_track')
        current_segment = train_data.get('current_segment')

        # Update the previously occupied segment
        self.previous_segments[train_name] = (current_track, current_segment)

    def draw_signal_circle(self, surface, row, col, radius_in_cells, color):
        """
        Draws a circle on the given surface.

        :param surface:   The surface (SubSurface or main screen) to draw on.
        :param row:       The grid row where the signal will be drawn.
        :param col:       The grid column where the signal will be drawn.
        :param radius_in_cells: The radius of the circle in terms of grid cells.
        :param color:     The color of the circle (R, G, B) or (R, G, B, A).
        """

        # Translate (row, col) from grid coordinates to pixel coordinates
        center_x = int(col * CELL_SIZE)
        center_y = int(row * CELL_SIZE)

        # Convert radius in grid cells to radius in pixels
        radius_pixels = int(radius_in_cells * CELL_SIZE)

        # Draw the circle with the provided color
        pygame.draw.circle(surface, color, (center_x, center_y), radius_pixels)

    def handle_starter_signal(self, surface, signal_id, status_id, radius_in_cells=1):
        """
        Handles drawing a starter signal based on signal_id (location)
        and status_id (color). Calls draw_signal_circle internally.

        :param surface:        The pygame surface to draw on (e.g. canvas_area).
        :param signal_id:      An integer identifying which signal to draw (1..5).
        :param status_id:      An integer identifying the color/state (0,1,2).
        :param radius_in_cells:Radius of the circle in grid cells (default=2).
        """

        # Dictionary for the 5 signals
        signal_positions = {
            1: (68, 90),
            2: (58, 105),
            3: (58, 195),
            4: (72, 95),
            5: (72, 195)
        }

        # Dictionary for the status -> color mapping
        status_colors = {
            0: (0, 255, 0),  # green
            1: (255, 0, 0),  # red
            2: (255, 255, 0)  # yellow
        }

        # Get the row-col from the signal_positions dictionary
        if signal_id not in signal_positions:
            print(f"Signal ID {signal_id} is not defined in signal_positions.")
            return

        row, col = signal_positions[signal_id]

        # Get the circle color from the status_colors dictionary
        if status_id not in status_colors:
            print(f"Status ID {status_id} is not defined in status_colors.")
            return

        color = status_colors[status_id]

        # Draw the circle
        self.draw_signal_circle(surface, row, col, radius_in_cells, color)



