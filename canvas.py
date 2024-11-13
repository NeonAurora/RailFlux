# canvas.py
import pygame
from train import Train
from track import Track
from settings import GRID_ROWS, GRID_COLUMNS, CELL_SIZE, TRAIN_COLOR, SPEED, GRID_BORDER_COLOR, GRID_BORDER_THICKNESS, SECOND_PRIORITY_COLOR

class Canvas:
    def __init__(self, width, height, bg_color, header_height):
        self.width = width
        self.height = height
        self.bg_color = bg_color
        self.header_height = header_height

        # Create a Track instance to draw multiple tracks (assumed track_id as 'T1' for now)
        self.track = Track(GRID_ROWS, GRID_COLUMNS, CELL_SIZE, track_id='T1')

        # Initialize the train to follow the waypoints defined by the track segments
        self.train = Train(CELL_SIZE * 1.5, CELL_SIZE * 1.5, TRAIN_COLOR, SPEED, self.track.get_all_waypoints())

    def draw_grid(self, canvas):
        temp_surface = pygame.Surface((self.width, self.height), pygame.SRCALPHA)
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
        canvas_rect = pygame.Rect(0, self.header_height, self.width, self.height)
        pygame.draw.rect(screen, self.bg_color, canvas_rect)

        canvas_area = screen.subsurface(canvas_rect)
        self.draw_grid(canvas_area)
        self.track.draw_tracks(canvas_area)
        self.train.draw(canvas_area)

    def update(self, screen):
        self.train.move()
        current_position = self.train.get_current_position()
        current_segment_id = self.track.get_segment_id_from_position(current_position)
        if current_segment_id:
            self.track.mark_segment_occupied(current_segment_id)
        self.draw(screen)
