import pygame

class Train:
    def __init__(self, width, height, color, speed, initial_position, direction):
        self.width = width
        self.height = height
        self.color = color
        self.speed = speed
        self.position = initial_position
        self.direction = direction

    def move(self, screen_width, screen_height, cell_size):
        if self.direction == "right":
            self.position[0] += self.speed
            if self.position[0] >= screen_width - cell_size:
                self.direction = "down"
        elif self.direction == "down":
            self.position[1] += self.speed
            if self.position[1] >= screen_height - cell_size * 2:
                self.direction = "left"
        elif self.direction == "left":
            self.position[0] -= self.speed
            if self.position[0] <= 0:
                self.direction = "up"
        elif self.direction == "up":
            self.position[1] -= self.speed
            if self.position[1] <= 0:
                self.direction = "right"

    def draw(self, screen):
        train_rect = pygame.Rect(self.position[0], self.position[1], self.width, self.height)
        pygame.draw.rect(screen, self.color, train_rect)
