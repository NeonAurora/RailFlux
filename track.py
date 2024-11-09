# track.py
import pygame

class Track:
    def __init__(self, grid_rows, grid_columns, cell_size):
        self.grid_rows = grid_rows
        self.grid_columns = grid_columns
        self.cell_size = cell_size

    def draw_track(self, canvas):
        # Define track using multiple cell coordinates
        points = [
            (60, 0),
            (60, 10),
            (60, 80),
            (50, 90),
            (50,140),
            (40,150),
            (40,170),
            (50, 180),
            (50, 200),
            (60, 210),
            (60, 230),
            (60, 250),
        ]

        # Convert cell coordinates to pixel coordinates
        pixel_points = [
            (col * self.cell_size, row * self.cell_size) for row, col in points
        ]

        # Draw lines between consecutive points in pixel_points
        track_color = (0, 255, 255)  # Cyan color
        track_thickness = 3  # Thickness of the track line

        # Iterate through the points and draw lines between consecutive points
        for i in range(len(pixel_points) - 1):
            start_pos = pixel_points[i]
            end_pos = pixel_points[i + 1]
            pygame.draw.line(canvas, track_color, start_pos, end_pos, track_thickness)
