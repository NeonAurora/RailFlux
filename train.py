import pygame
import math
from collections import deque


class Train:
    def __init__(self, width, height, color, speed, waypoints, num_wagons=10, spacing_interval=3):
        self.width = width
        self.height = height
        self.color = color
        self.speed = speed
        self.waypoints = waypoints  # List of waypoints (in pixel coordinates)
        self.current_waypoint_index = 0
        self.position = list(waypoints[0])  # Start at the first waypoint

        # Train segments: head + wagons
        self.num_wagons = num_wagons
        self.positions = deque(maxlen=(num_wagons + 1) * spacing_interval)  # Stores positions of head and wagons
        self.positions.append(self.position[:])  # Initialize with the head's starting position
        self.spacing_interval = spacing_interval  # Number of frames between recording positions
        self.frame_counter = 0  # Counter to track frames

    def move(self):
        # If there are no waypoints, do nothing
        if len(self.waypoints) == 0:
            return

        # Get the current target waypoint
        target_x, target_y = self.waypoints[self.current_waypoint_index]

        # Calculate the difference in x and y from current position to target waypoint
        delta_x = target_x - self.position[0]
        delta_y = target_y - self.position[1]

        # Calculate distance to target waypoint
        distance = math.sqrt(delta_x ** 2 + delta_y ** 2)

        # If we have reached the current waypoint, move to the next one
        if distance < self.speed:
            self.position = [target_x, target_y]
            self.current_waypoint_index += 1

            # If we reached the last waypoint, reset to the first waypoint
            if self.current_waypoint_index >= len(self.waypoints):
                self.current_waypoint_index = 0
                self.position = list(self.waypoints[0])  # Reset position to the first waypoint

            return

        # Calculate the direction vector and normalize it
        direction_x = delta_x / distance
        direction_y = delta_y / distance

        # Move the head of the train in the direction of the target waypoint
        self.position[0] += direction_x * self.speed
        self.position[1] += direction_y * self.speed

        # Add the current position to the list of positions, but only at the specified spacing interval
        self.frame_counter += 1
        if self.frame_counter >= self.spacing_interval:
            self.positions.append(self.position[:])  # Add the current head position to the positions deque
            self.frame_counter = 0  # Reset the frame counter

    def draw(self, screen):
        # Draw each segment of the train (head + wagons)
        # Get the positions of the wagons based on spacing from the deque
        positions_list = list(self.positions)
        for i in range(0, len(positions_list), self.spacing_interval):
            pos = positions_list[i]
            train_rect = pygame.Rect(pos[0], pos[1], self.width, self.height)
            pygame.draw.rect(screen, self.color, train_rect)
