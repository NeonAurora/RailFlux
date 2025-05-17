# train_detection_icon.py
import pygame


class TrainDetectionIcon:
    def __init__(self, cell_size):
        self.cell_size = cell_size
        # Adjust these scaling factors to match your desired proportions
        self.width = int(cell_size * 5.0)  # Width of main rounded rectangle
        self.height = int(cell_size * 1.6)  # Height of main rounded rectangle
        self.circle_radius = int(cell_size * 0.5)  # Radius of the circles
        self.stem_width = int(cell_size * 0.9)  # Width of the vertical stem
        self.outline_thickness = 1  # Control the thickness of all outlines

    def draw(self, surface, x, y, active=True):
        """
        Draw the train detection icon at specified coordinates.
        :param surface: The pygame surface to draw on
        :param x, y: Center coordinates
        :param active: If True, the left circle is filled red, otherwise transparent with white outline
        """
        # Calculate positions
        rect_left = x - self.width // 2
        rect_top = y - self.height // 2

        # Draw the stem (vertical line on left) - white
        stem_rect = pygame.Rect(
            rect_left - self.stem_width,
            y - self.stem_width // 2,
            self.stem_width,
            self.stem_width
        )
        pygame.draw.rect(surface, (255, 255, 255), stem_rect)  # White stem

        # Draw ONLY the outline of the rounded rectangle container - in white
        # with controllable thickness
        container_rect = pygame.Rect(rect_left, rect_top, self.width, self.height)
        border_radius = self.height // 2  # Fully rounded ends
        pygame.draw.rect(
            surface,
            (255, 255, 255),  # White outline
            container_rect,
            self.outline_thickness,  # Using the thickness parameter
            border_radius=border_radius
        )

        # Calculate circle positions (4 circles evenly spaced)
        spacing = self.width / 5  # Divide width into 5 parts to get 4 evenly spaced positions

        # Draw the leftmost circle (red if active, otherwise transparent with white outline)
        if active:
            # Filled red circle
            pygame.draw.circle(
                surface,
                (255, 0, 0),  # Red fill
                (rect_left + spacing, y),
                self.circle_radius
            )
        else:
            # Just white outline
            pygame.draw.circle(
                surface,
                (255, 255, 255),  # White outline
                (rect_left + spacing, y),
                self.circle_radius,
                self.outline_thickness  # Using the thickness parameter
            )

        # Draw the 3 circles with white outlines and transparent centers
        for i in range(1, 4):
            pygame.draw.circle(
                surface,
                (255, 255, 255),  # White outline (fixed from black to white)
                (rect_left + spacing * (i + 1), y),
                self.circle_radius,
                self.outline_thickness  # Using the thickness parameter
            )