import pygame

class Canvas:
    def __init__(self, width, height, bg_color):
        self.width = width
        self.height = height
        self.bg_color = bg_color
        self.screen = pygame.display.set_mode((self.width, self.height))
        pygame.display.set_caption("Simple 6x6 Loop Track with Moving Train")

    def clear(self):
        self.screen.fill(self.bg_color)

    def update(self):
        pygame.display.flip()
