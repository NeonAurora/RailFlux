# train.py
import pygame
import math
from collections import deque

class Train:
    def __init__(self, width, height, color, speed, waypoints, num_wagons=10, spacing_interval=3):
        self.width = width
        self.height = height
        self.color = color
        self.speed = speed
        self.waypoints = waypoints
        self.current_waypoint_index = 0
        self.position = list(waypoints[0])

        self.num_wagons = num_wagons
        self.positions = deque(maxlen=(num_wagons + 1) * spacing_interval)
        self.positions.append(self.position[:])
        self.spacing_interval = spacing_interval
        self.frame_counter = 0

    def move(self):
        if len(self.waypoints) == 0:
            return

        target_x, target_y = self.waypoints[self.current_waypoint_index]
        delta_x = target_x - self.position[0]
        delta_y = target_y - self.position[1]
        distance = math.sqrt(delta_x ** 2 + delta_y ** 2)

        if distance < self.speed:
            self.position = [target_x, target_y]
            self.current_waypoint_index += 1
            if self.current_waypoint_index >= len(self.waypoints):
                self.current_waypoint_index = 0
                self.position = list(self.waypoints[0])
            return

        direction_x = delta_x / distance
        direction_y = delta_y / distance
        self.position[0] += direction_x * self.speed
        self.position[1] += direction_y * self.speed

        self.frame_counter += 1
        if self.frame_counter >= self.spacing_interval:
            self.positions.append(self.position[:])
            self.frame_counter = 0

    def draw(self, screen):
        positions_list = list(self.positions)
        for i in range(0, len(positions_list), self.spacing_interval):
            pos = positions_list[i]
            train_rect = pygame.Rect(pos[0], pos[1], self.width, self.height)
            pygame.draw.rect(screen, self.color, train_rect)

    def get_current_position(self):
        return self.position
