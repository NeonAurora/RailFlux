# track.py
import pygame
import firebase_setup  # Import firebase_setup to ensure Firebase is initialized
from firebase_admin import db
import settings
from settings import TRACK_COLOR


class Track:
    def __init__(self, grid_rows, grid_columns, cell_size):
        self.grid_rows = grid_rows
        self.grid_columns = grid_columns
        self.cell_size = cell_size
        self.tracks = {}  # Store all tracks data
        self.gap_size = 5  # Define gap size in pixels

        # Firebase reference to the tracks data
        self.tracks_ref = db.reference('/tracks')

        # Load all tracks data from Firebase
        self.load_tracks_from_firebase()

    def load_tracks_from_firebase(self):
        # Load all tracks data from Firebase
        tracks_data = self.tracks_ref.get()
        if tracks_data:
            for track_id, track_info in tracks_data.items():
                segments = track_info.get('segments', {})
                self.tracks[track_id] = {}
                for segment_id, segment_info in segments.items():
                    segment = {
                        'coordinates': segment_info['coordinates'],
                        'occupied': segment_info['occupied']
                    }
                    self.tracks[track_id][segment_id] = segment

    def draw_tracks(self, canvas):
        unoccupied_color = TRACK_COLOR  # Cyan for unoccupied segments
        occupied_color = (255, 0, 0)      # Red for occupied segments
        track_thickness = 3

        # Iterate over each track and draw each segment with gaps between segments
        for track_id, segments in self.tracks.items():
            for segment_id, segment_info in segments.items():
                color = occupied_color if segment_info['occupied'] else unoccupied_color
                coordinates = segment_info['coordinates']
                pixel_coordinates = [(col * self.cell_size, row * self.cell_size) for row, col in coordinates]

                # Draw each segment with a gap
                for i in range(len(pixel_coordinates) - 1):
                    start_pos = pixel_coordinates[i]
                    end_pos = pixel_coordinates[i + 1]

                    # Adjust start and end positions to create gaps
                    segment_start = self._adjust_position(start_pos, end_pos, self.gap_size, is_start=True)
                    segment_end = self._adjust_position(start_pos, end_pos, self.gap_size, is_start=False)

                    # Draw the adjusted line segment with a gap
                    pygame.draw.line(canvas, color, segment_start, segment_end, track_thickness)

    def _adjust_position(self, start, end, gap_size, is_start=True):
        """
        Adjusts the position of the segment to create a gap.
        :param start: Starting coordinate of the line (x, y)
        :param end: Ending coordinate of the line (x, y)
        :param gap_size: Size of the gap in pixels
        :param is_start: If True, adjust the start point. If False, adjust the end point.
        :return: Adjusted coordinate (x, y)
        """
        # Calculate direction vector from start to end
        delta_x = end[0] - start[0]
        delta_y = end[1] - start[1]
        distance = (delta_x ** 2 + delta_y ** 2) ** 0.5

        # Normalize the direction vector
        direction_x = delta_x / distance
        direction_y = delta_y / distance

        # Adjust the position to add a gap
        if is_start:
            adjusted_x = start[0] + direction_x * gap_size
            adjusted_y = start[1] + direction_y * gap_size
        else:
            adjusted_x = end[0] - direction_x * gap_size
            adjusted_y = end[1] - direction_y * gap_size

        return (adjusted_x, adjusted_y)

    def get_track_waypoints(self, track_id, reverse=False):
        """
        Get all waypoints for a specific track.
        :param track_id: The ID of the track.
        :param reverse: If True, reverse the waypoints for moving backward.
        :return: List of waypoints (coordinates) for the track.
        """
        waypoints = []
        if track_id in self.tracks:
            for segment_info in self.tracks[track_id].values():
                for coordinate in segment_info['coordinates']:
                    pixel_coordinate = (coordinate[1] * self.cell_size, coordinate[0] * self.cell_size)
                    waypoints.append(pixel_coordinate)

        if reverse:
            waypoints = waypoints[::-1]

        return waypoints

    def get_segment_id_from_position(self, position):
        px, py = position

        for track_id, segments in self.tracks.items():
            for segment_id, segment_info in segments.items():
                coordinates = segment_info['coordinates']
                # Convert cell coordinates to pixel coordinates
                pixel_coordinates = [(col * self.cell_size, row * self.cell_size) for row, col in coordinates]

                # Check if the current position falls between two consecutive coordinates in the segment
                for i in range(len(pixel_coordinates) - 1):
                    start = pixel_coordinates[i]
                    end = pixel_coordinates[i + 1]

                    if self._is_position_on_segment((px, py), start, end):
                        return track_id, segment_id

        return None

    def _is_position_on_segment(self, position, start, end):
        px, py = position
        sx, sy = start
        ex, ey = end

        # Calculate cross product to check if position is on the line
        cross_product = (py - sy) * (ex - sx) - (px - sx) * (ey - sy)
        if abs(cross_product) > 1e-6:
            return False

        # Check if position lies within the segment bounds
        if min(sx, ex) <= px <= max(sx, ex) and min(sy, ey) <= py <= max(sy, ey):
            return True

        return False

    def mark_segment_occupied(self, track_id, segment_id):
        if track_id in self.tracks and segment_id in self.tracks[track_id]:
            segment = self.tracks[track_id][segment_id]
            if not segment['occupied']:
                segment['occupied'] = True
                self.tracks_ref.child(f'{track_id}/segments/{segment_id}/occupied').set(True)

    def mark_segment_free(self, track_id, segment_id):
        if track_id in self.tracks and segment_id in self.tracks[track_id]:
            segment = self.tracks[track_id][segment_id]
            if segment['occupied']:
                segment['occupied'] = False
                self.tracks_ref.child(f'{track_id}/segments/{segment_id}/occupied').set(False)
