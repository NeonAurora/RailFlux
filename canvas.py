# canvas.py
import pygame
from track import Track
from train import Train
from settings import GRID_ROWS, GRID_COLUMNS, CELL_SIZE, TRAIN_COLOR, SPEED, GRID_BORDER_COLOR, GRID_BORDER_THICKNESS

class Canvas:
    def __init__(self, width, height, bg_color, header_height):
        self.width = width
        self.height = height
        self.bg_color = bg_color
        self.header_height = header_height
        self.track = Track(GRID_ROWS, GRID_COLUMNS, CELL_SIZE)  # Track initialized without specific color for grid
        self.train = Train(CELL_SIZE, CELL_SIZE * 2, TRAIN_COLOR, SPEED, [1, 1], "right")

    def draw_grid(self, canvas):
        # Draw grid lines with custom color and thickness
        for row in range(GRID_ROWS):
            for col in range(GRID_COLUMNS):
                cell_rect = pygame.Rect(col * CELL_SIZE, row * CELL_SIZE, CELL_SIZE, CELL_SIZE)
                pygame.draw.rect(canvas, GRID_BORDER_COLOR, cell_rect, GRID_BORDER_THICKNESS)

    def draw(self, screen):
        # Draw canvas background
        canvas_rect = pygame.Rect(0, self.header_height, self.width, self.height)
        pygame.draw.rect(screen, self.bg_color, canvas_rect)

        # Define a canvas area and draw grid, track, and train on it
        canvas_area = screen.subsurface(canvas_rect)
        self.draw_grid(canvas_area)  # Draw the grid using custom grid border color and thickness
        self.track.draw(canvas_area)
        self.train.draw(canvas_area)

    def update(self, screen):
        # Update train position and redraw the canvas
        self.train.move(self.width, self.height, CELL_SIZE)
        self.draw(screen)
