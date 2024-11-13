# canvas.py
import pygame
from train import Train
from track import Track
from settings import (GRID_ROWS, GRID_COLUMNS, CELL_SIZE, TRAIN_COLOR, TRAIN2_COLOR, SPEED,
                      GRID_BORDER_COLOR, GRID_BORDER_THICKNESS, SECOND_PRIORITY_COLOR, TRAIN_TRANSPARENT)

class Canvas:
    def __init__(self, width, height, bg_color, header_height):
        self.width = width
        self.height = height
        self.bg_color = bg_color
        self.header_height = header_height

        # Create a Track instance to draw multiple tracks
        self.track = Track(GRID_ROWS, GRID_COLUMNS, CELL_SIZE)

        # Initialize two trains, each assigned to a separate track with unique train IDs
        # Use TRAIN_TRANSPARENT to determine the train color (if transparent, make alpha 0)
        self.train1_color = (*TRAIN_COLOR[:3], 0) if TRAIN_TRANSPARENT else TRAIN_COLOR
        self.train2_color = (*TRAIN2_COLOR[:3], 0) if TRAIN_TRANSPARENT else TRAIN2_COLOR

        # Initialize two trains with unique IDs
        self.train1 = Train(CELL_SIZE * 1.5, CELL_SIZE * 1.5, self.train1_color, SPEED, self.track.get_track_waypoints('T1'), train_id='train1')
        self.train2 = Train(CELL_SIZE * 1.5, CELL_SIZE * 1.5, self.train2_color, SPEED, self.track.get_track_waypoints('T2', reverse=True), train_id='train2')

        # Keep track of the previously occupied segments for each train
        self.previous_segments = {
            'train1': None,
            'train2': None
        }

    def draw_grid(self, canvas):
        temp_surface = pygame.Surface((self.width, self.height), pygame.SRCALPHA)

        # Draw horizontal and vertical grid lines
        for row in range(GRID_ROWS + 1):
            y = row * CELL_SIZE
            color = SECOND_PRIORITY_COLOR if row % 10 == 0 else GRID_BORDER_COLOR
            pygame.draw.line(temp_surface, color, (0, y), (self.width, y), GRID_BORDER_THICKNESS)

        for col in range(GRID_COLUMNS + 1):
            x = col * CELL_SIZE
            color = SECOND_PRIORITY_COLOR if col % 10 == 0 else GRID_BORDER_COLOR
            pygame.draw.line(temp_surface, color, (x, 0), (x, self.height), GRID_BORDER_THICKNESS)

        canvas.blit(temp_surface, (0, 0))

    def draw(self, screen):
        # Draw canvas background
        canvas_rect = pygame.Rect(0, self.header_height, self.width, self.height)
        pygame.draw.rect(screen, self.bg_color, canvas_rect)

        # Define the canvas area and draw grid, tracks, and trains
        canvas_area = screen.subsurface(canvas_rect)
        self.draw_grid(canvas_area)
        self.track.draw_tracks(canvas_area)
        self.train1.draw(canvas_area)
        self.train2.draw(canvas_area)

    def update(self, screen):
        # Update both trains
        self.train1.move()
        self.train2.move()

        # Update segment occupation for train 1
        self.update_train_segment('train1', self.train1)

        # Update segment occupation for train 2
        self.update_train_segment('train2', self.train2)

        # Redraw everything on the screen
        self.draw(screen)

    def update_train_segment(self, train_name, train):
        current_position = train.get_current_position()
        current_segment_info = self.track.get_segment_id_from_position(current_position)

        # If we have a previous segment and it's different from the current one, mark it as free
        if self.previous_segments[train_name] and self.previous_segments[train_name] != current_segment_info:
            previous_track_id, previous_segment_id = self.previous_segments[train_name]
            self.track.mark_segment_free(previous_track_id, previous_segment_id)

        # If there is a current segment, mark it as occupied
        if current_segment_info:
            current_track_id, current_segment_id = current_segment_info
            self.track.mark_segment_occupied(current_track_id, current_segment_id)

        # Update the previous segment
        self.previous_segments[train_name] = current_segment_info
