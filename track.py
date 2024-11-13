# track.py
import pygame
import firebase_setup  # Import firebase_setup to ensure Firebase is initialized
from firebase_admin import db


class Track:
    def __init__(self, grid_rows, grid_columns, cell_size, track_id):
        self.grid_rows = grid_rows
        self.grid_columns = grid_columns
        self.cell_size = cell_size
        self.track_id = track_id
        self.segments = {}
        self.gap_size = 5  # Define gap size in pixels

        # Firebase reference to the track data
        self.track_ref = db.reference(f'/tracks/{track_id}')

        # Load segments data from Firebase
        self.load_segments_from_firebase()

    def load_segments_from_firebase(self):
        # Load segments data from Firebase
        segments_data = self.track_ref.child('segments').get()
        if segments_data:
            for segment_id, segment_info in segments_data.items():
                segment = {
                    'coordinates': segment_info['coordinates'],
                    'occupied': segment_info['occupied']
                }
                self.segments[segment_id] = segment

    def draw_tracks(self, canvas):
        unoccupied_color = (0, 255, 255)  # Cyan for unoccupied segments
        occupied_color = (255, 0, 0)  # Red for occupied segments
        track_thickness = 3

        # Iterate over each segment and draw it with gaps between segments
        for segment_id, segment_info in self.segments.items():
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

    def get_all_waypoints(self):
        """
        Collect all waypoints from all segments to use for train movement.
        :return: List of waypoints (coordinates) for the train to follow.
        """
        waypoints = []
        for segment_info in self.segments.values():
            # Append each coordinate in the segment as a waypoint
            for coordinate in segment_info['coordinates']:
                pixel_coordinate = (coordinate[1] * self.cell_size, coordinate[0] * self.cell_size)
                waypoints.append(pixel_coordinate)

        return waypoints

    def get_segment_id_from_position(self, position):
        """
        Determine which segment the given position falls into.
        :param position: Current position of the train (x, y)
        :return: Segment ID if found, otherwise None
        """
        px, py = position

        for segment_id, segment_info in self.segments.items():
            coordinates = segment_info['coordinates']
            # Convert cell coordinates to pixel coordinates
            pixel_coordinates = [(col * self.cell_size, row * self.cell_size) for row, col in coordinates]

            # Check if the current position falls between two consecutive coordinates in the segment
            for i in range(len(pixel_coordinates) - 1):
                start = pixel_coordinates[i]
                end = pixel_coordinates[i + 1]

                if self._is_position_on_segment((px, py), start, end):
                    return segment_id

        return None

    def _is_position_on_segment(self, position, start, end):
        """
        Check if a given position falls on the line segment between start and end.
        :param position: Current position (x, y)
        :param start: Start coordinate of the line (x, y)
        :param end: End coordinate of the line (x, y)
        :return: True if the position is on the segment, False otherwise
        """
        px, py = position
        sx, sy = start
        ex, ey = end

        # Calculate cross product to check if position is on the line
        cross_product = (py - sy) * (ex - sx) - (px - sx) * (ey - sy)
        if abs(cross_product) > 1e-6:  # Tolerance for floating-point errors
            return False

        # Check if position lies within the segment bounds
        if min(sx, ex) <= px <= max(sx, ex) and min(sy, ey) <= py <= max(sy, ey):
            return True

        return False

    def mark_segment_occupied(self, segment_id):
        """
        Mark a segment as occupied.
        :param segment_id: The ID of the segment to mark as occupied.
        """
        if segment_id in self.segments and not self.segments[segment_id]['occupied']:
            self.segments[segment_id]['occupied'] = True
            self.track_ref.child(f'segments/{segment_id}/occupied').set(True)

    def mark_segment_free(self, segment_id):
        """
        Mark a segment as free.
        :param segment_id: The ID of the segment to mark as free.
        """
        if segment_id in self.segments and self.segments[segment_id]['occupied']:
            self.segments[segment_id]['occupied'] = False
            self.track_ref.child(f'segments/{segment_id}/occupied').set(False)
