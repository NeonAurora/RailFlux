# track.py
import pygame

class Track:
    def __init__(self, grid_rows, grid_columns, cell_size):
        self.grid_rows = grid_rows
        self.grid_columns = grid_columns
        self.cell_size = cell_size

        # Define multiple tracks using waypoints (in cell coordinates)
        # Each track has a set of waypoints
        self.tracks = [
            [  # Track T1
                (60, 0),
                (60, 250)
            ],
            [  # Track T2
                (80, 0),
                (80, 250)
            ]
        ]

    def draw_tracks(self, canvas):
        track_color = (0, 255, 255)  # Cyan color
        track_thickness = 3  # Thickness of the track line

        # Iterate over each set of waypoints to draw multiple tracks
        for track_waypoints in self.tracks:
            # Convert cell coordinates to pixel coordinates
            pixel_waypoints = [
                (col * self.cell_size, row * self.cell_size) for row, col in track_waypoints
            ]

            # Iterate through the points and draw lines between consecutive points
            for i in range(len(pixel_waypoints) - 1):
                start_pos = pixel_waypoints[i]
                end_pos = pixel_waypoints[i + 1]
                pygame.draw.line(canvas, track_color, start_pos, end_pos, track_thickness)
