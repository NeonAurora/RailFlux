# track.py
import pygame
from settings import GRID_ROWS, GRID_COLUMNS, CELL_SIZE, TRACK_COLOR

class Track:
    def __init__(self, grid_rows, grid_columns, cell_size, color):
        self.grid_rows = grid_rows
        self.grid_columns = grid_columns
        self.cell_size = cell_size
        self.color = color

    def draw(self, canvas):
        # Draw the grid inside the canvas area
        for row in range(self.grid_rows):
            for col in range(self.grid_columns):
                cell_rect = pygame.Rect(col * self.cell_size, row * self.cell_size, self.cell_size, self.cell_size)
                pygame.draw.rect(canvas, self.color, cell_rect, 1)  # 1 px border