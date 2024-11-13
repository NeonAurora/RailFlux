# canvas.py
import pygame
from train import Train
from track import Track
from settings import GRID_ROWS, GRID_COLUMNS, CELL_SIZE, TRAIN_COLOR, SPEED, GRID_BORDER_COLOR, GRID_BORDER_THICKNESS, SECOND_PRIORITY_COLOR

class Canvas:
    def __init__(self, width, height, bg_color, header_height):
        self.width = width
        self.height = height
        self.bg_color = bg_color
        self.header_height = header_height

        # Create a Track instance to draw multiple tracks
        self.track = Track(GRID_ROWS, GRID_COLUMNS, CELL_SIZE)

        # Define waypoints for the train (following track T1 for now)
        cell_waypoints = [
            (60, 0),   # Point 1: Cell coordinates (row, column)
            (60, 10),  # Point 2
            (60, 80),  # Point 3
            (50, 90),  # Point 4
            (50, 140), # Point 5
            (40, 150), # Point 6
            (40, 170), # Point 7
            (50, 180), # Point 8
            (50, 200), # Point 9
            (60, 210), # Point 10
            (60, 230), # Point 11
            (60, 250)  # Point 12
        ]

        # Convert cell coordinates to pixel coordinates
        self.train_waypoints = [
            (col * CELL_SIZE, row * CELL_SIZE) for row, col in cell_waypoints
        ]

        # Initialize the train with the waypoints
        self.train = Train(CELL_SIZE * 1.5, CELL_SIZE * 1.5, TRAIN_COLOR, SPEED, self.train_waypoints)

    def draw_grid(self, canvas):
        # Create a temporary surface to draw the grid with transparency
        temp_surface = pygame.Surface((self.width, self.height), pygame.SRCALPHA)  # Enable alpha channel

        # Draw vertical and horizontal lines for grid with different colors for every 10th line
        for row in range(GRID_ROWS + 1):
            # Draw horizontal lines
            y = row * CELL_SIZE
            color = SECOND_PRIORITY_COLOR if row % 10 == 0 else GRID_BORDER_COLOR
            pygame.draw.line(temp_surface, color, (0, y), (self.width, y), GRID_BORDER_THICKNESS)

        for col in range(GRID_COLUMNS + 1):
            # Draw vertical lines
            x = col * CELL_SIZE
            color = SECOND_PRIORITY_COLOR if col % 10 == 0 else GRID_BORDER_COLOR
            pygame.draw.line(temp_surface, color, (x, 0), (x, self.height), GRID_BORDER_THICKNESS)

        # Blit the temporary surface with grid onto the canvas
        canvas.blit(temp_surface, (0, 0))

    def draw(self, screen):
        # Draw canvas background
        canvas_rect = pygame.Rect(0, self.header_height, self.width, self.height)
        pygame.draw.rect(screen, self.bg_color, canvas_rect)

        # Define a canvas area and draw grid, tracks, and train on it
        canvas_area = screen.subsurface(canvas_rect)
        self.draw_grid(canvas_area)  # Draw the grid using custom grid border color and thickness
        self.track.draw_tracks(canvas_area)  # Draw all the tracks on the canvas
        self.train.draw(canvas_area)  # Draw the train on the canvas

    def update(self, screen):
        # Update train position and redraw the canvas
        self.train.move()
        self.draw(screen)
