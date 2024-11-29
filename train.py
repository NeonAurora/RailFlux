# train.py
import pygame
import math
import time
from collections import deque
from firebase_admin import db

class Train:
    def __init__(self, width, height, color, speed, waypoints, num_wagons=10, spacing_interval=3, train_id='train1'):
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
        self.train_id = train_id  # Unique identifier for each train

        # Firebase reference for the train's data, each train has a unique path
        self.train_ref = db.reference(f'/trains/{self.train_id}')

        # Timer to control Firebase update frequency
        self.last_update_time = time.time()

    def move(self):
        # Return if no waypoints are defined
        if len(self.waypoints) == 0:
            return

        # Calculate the difference between current position and target waypoint
        target_x, target_y = self.waypoints[self.current_waypoint_index]
        delta_x = target_x - self.position[0]
        delta_y = target_y - self.position[1]
        distance = math.sqrt(delta_x ** 2 + delta_y ** 2)

        # If the train has reached the current waypoint, move to the next one
        if distance < self.speed:
            self.position = [target_x, target_y]
            self.current_waypoint_index += 1
            if self.current_waypoint_index >= len(self.waypoints):
                # Reset to the first waypoint after reaching the last waypoint
                self.current_waypoint_index = 0
                self.position = list(self.waypoints[0])
            return

        # Normalize the direction vector and update the position of the train
        direction_x = delta_x / distance
        direction_y = delta_y / distance
        self.position[0] += direction_x * self.speed
        self.position[1] += direction_y * self.speed

        # Append the position to the list for wagons to follow
        self.frame_counter += 1
        if self.frame_counter >= self.spacing_interval:
            self.positions.append(self.position[:])
            self.frame_counter = 0

        # Update Firebase every second
        current_time = time.time()
        if current_time - self.last_update_time >= 1:  # Check if at least 1 second has passed
            # self.update_firebase()
            self.last_update_time = current_time

    def update_firebase(self):
        # Write current position to Firebase
        self.train_ref.update({
            'current_position': {
                'x': self.position[0],
                'y': self.position[1]
            },
            'timestamp': time.time()
        })

    def draw(self, screen):
        # Create a temporary surface to support transparency
        temp_surface = pygame.Surface((self.width, self.height), pygame.SRCALPHA)

        # Draw each segment of the train (head + wagons)
        positions_list = list(self.positions)
        for i in range(0, len(positions_list), self.spacing_interval):
            pos = positions_list[i]
            train_rect = pygame.Rect(0, 0, self.width, self.height)

            # Use the temporary surface to draw with transparency
            temp_surface.fill(self.color)
            screen.blit(temp_surface, pos)

    def get_current_position(self):
        return self.position
