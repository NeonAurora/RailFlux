# canvas.py
import pygame
from train import Train
from settings import GRID_ROWS, GRID_COLUMNS, CELL_SIZE, TRAIN_COLOR, SPEED, GRID_BORDER_COLOR, GRID_BORDER_THICKNESS, SECOND_PRIORITY_COLOR

class Canvas:
    def __init__(self, width, height, bg_color, header_height):
        self.width = width
        self.height = height
        self.bg_color = bg_color
        self.header_height = header_height
        self.train = Train(CELL_SIZE, CELL_SIZE * 2, TRAIN_COLOR, SPEED, [1, 1], "right")

    def draw_grid(self, canvas):
        # Create a temporary surface to draw the grid with transparency
        temp_surface = pygame.Surface((self.width, self.height), pygame.SRCALPHA)  # Enable alpha channel

        # Draw grid lines with custom color and thickness
        for row in range(GRID_ROWS):
            for col in range(GRID_COLUMNS):
                cell_rect = pygame.Rect(col * CELL_SIZE, row * CELL_SIZE, CELL_SIZE, CELL_SIZE)

                # Use SECOND_PRIORITY_COLOR for every 10th row and column
                if row % 10 == 0 or col % 10 == 0:
                    pygame.draw.rect(temp_surface, SECOND_PRIORITY_COLOR, cell_rect, GRID_BORDER_THICKNESS)
                else:
                    pygame.draw.rect(temp_surface, GRID_BORDER_COLOR, cell_rect, GRID_BORDER_THICKNESS)

        # Blit the temporary surface with grid onto the canvas
        canvas.blit(temp_surface, (0, 0))

    def draw(self, screen):
        # Draw canvas background
        canvas_rect = pygame.Rect(0, self.header_height, self.width, self.height)
        pygame.draw.rect(screen, self.bg_color, canvas_rect)

        # Define a canvas area and draw grid and train on it
        canvas_area = screen.subsurface(canvas_rect)
        self.draw_grid(canvas_area)  # Draw the grid using custom grid border color and thickness
        self.train.draw(canvas_area)

    def update(self, screen):
        # Update train position and redraw the canvas
        self.train.move(self.width, self.height, CELL_SIZE)
        self.draw(screen)
