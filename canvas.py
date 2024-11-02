# canvas.py
import pygame
from track import Track
from train import Train
from settings import GRID_ROWS, GRID_COLUMNS, CELL_SIZE, TRACK_COLOR, TRAIN_COLOR, SPEED


class Canvas:
    def __init__(self, width, height, bg_color, header_height):
        self.width = width
        self.height = height
        self.bg_color = bg_color
        self.header_height = header_height
        self.track = Track(GRID_ROWS, GRID_COLUMNS, CELL_SIZE, TRACK_COLOR)
        self.train = Train(CELL_SIZE, CELL_SIZE * 2, TRAIN_COLOR, SPEED, [1, 1], "right")

    def draw(self, screen):
        # Draw canvas background
        canvas_rect = pygame.Rect(0, self.header_height, self.width, self.height)
        pygame.draw.rect(screen, self.bg_color, canvas_rect)

        # Draw track and train on the canvas area
        canvas_area = screen.subsurface(canvas_rect)
        self.track.draw(canvas_area)
        self.train.draw(canvas_area)

    def update(self, screen):
        # Update train position and redraw canvas
        self.train.move(self.width, self.height, CELL_SIZE)
        self.draw(screen)
