import pygame

class Track:
    def __init__(self, grid_size, cell_size, color):
        self.grid_size = grid_size
        self.cell_size = cell_size
        self.color = color

    def draw(self, screen):
        for row in range(self.grid_size):
            for col in range(self.grid_size):
                cell_rect = pygame.Rect(col * self.cell_size, row * self.cell_size, self.cell_size, self.cell_size)
                pygame.draw.rect(screen, self.color, cell_rect, 2)  # 2 px border
