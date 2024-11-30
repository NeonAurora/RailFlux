# train.py
import pygame
import time
from firebase_admin import db

class Train:
    def __init__(self, width, height, color, speed, waypoints, num_wagons=10, spacing_interval=3, train_id='train1'):
        self.width = width
        self.height = height
        self.color = color
        self.speed = speed
        self.waypoints = waypoints
        self.current_waypoint_index = 0
        self.position = list(waypoints[0]) if waypoints else [0, 0]
        self.num_wagons = num_wagons
        self.spacing_interval = spacing_interval
        self.train_id = train_id  # Unique identifier for each train

        # Firebase reference for the train's data, each train has a unique path
        self.train_ref = db.reference(f'/trains/{self.train_id}')

        # Timer to control Firebase read frequency
        self.last_update_time = time.time()

    def fetch_data_from_firebase(self):
        # Update Firebase every second
        current_time = time.time()
        if current_time - self.last_update_time >= 1:  # Check if at least 1 second has passed
            train_data = self.train_ref.get()
            if train_data and 'current_position' in train_data:
                self.position[0] = train_data['current_position']['x']
                self.position[1] = train_data['current_position']['y']
            self.last_update_time = current_time

    def move(self):
        # Fetch updated position from Firebase
        self.fetch_data_from_firebase()

    def draw(self, screen):
        # Create a temporary surface to support transparency
        temp_surface = pygame.Surface((self.width, self.height), pygame.SRCALPHA)

        # Draw the train head
        train_rect = pygame.Rect(self.position[0], self.position[1], self.width, self.height)
        temp_surface.fill(self.color)
        screen.blit(temp_surface, (self.position[0], self.position[1]))

    def get_current_position(self):
        return self.position
