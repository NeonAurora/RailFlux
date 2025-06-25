# text_renderer.py
import pygame
import os
import sys  # This import was missing!


class TextRenderer:
    def __init__(self, default_font_size=16, cell_size=1.0):
        # Initialize the font
        pygame.font.init()
        self.default_font_size = int(default_font_size)
        self.font = pygame.font.SysFont('Arial', self.default_font_size)

        # Store text elements as a list of tuples (text, position, font_size)
        self.text_elements = []

        # Store cell size for grid-to-pixel conversion
        self.cell_size = cell_size

    def add_text(self, text, position, font_size=None):
        """
        Add a text element to be rendered at the specified position.
        :param text: String to display
        :param position: Tuple (x, y) coordinates where to display the text
        :param font_size: Optional font size (uses default if None)
        """
        if font_size is None:
            font_size = self.default_font_size
        else:
            font_size = int(font_size)  # Ensure integer font size

        # Ensure positions are integers
        position = (int(position[0]), int(position[1]))

        self.text_elements.append((text, position, font_size))

    def clear_texts(self):
        """Clear all text elements"""
        self.text_elements = []

    def load_from_file(self, filename):
        """
        Load text elements from a configuration file.
        File format: Text|GridY|GridX|FontSize(optional)
        """
        self.clear_texts()  # Clear existing text elements

        # Get the path to the file using resource_path
        file_path = self._resource_path(filename)

        try:
            with open(file_path, 'r') as file:
                for line in file:
                    line = line.strip()
                    # Skip empty lines and comments
                    if not line or line.startswith('#'):
                        continue

                    # Split the line into parts
                    parts = line.split('|')

                    # Basic validation
                    if len(parts) < 3:
                        print(f"Skipping invalid line: {line}")
                        continue

                    text = parts[0]

                    try:
                        # Convert grid coordinates to pixel coordinates
                        grid_y = float(parts[1])
                        grid_x = float(parts[2])
                        pixel_x = int(grid_x * self.cell_size)
                        pixel_y = int(grid_y * self.cell_size)

                        # Check if font size is specified
                        font_size = self.default_font_size
                        if len(parts) >= 4 and parts[3]:
                            font_size = int(parts[3])

                        # Add the text element
                        self.add_text(text, (pixel_x, pixel_y), font_size)

                    except ValueError as e:
                        print(f"Error parsing line: {line} - {e}")

            print(f"Successfully loaded {len(self.text_elements)} text elements from {filename}")

        except FileNotFoundError:
            print(f"Text configuration file not found: {filename}")
            print(f"Attempted to use file at: {file_path}")
            print(f"Current working directory: {os.getcwd()}")
            # List files in current directory to help debugging
            print("Files in current directory:", os.listdir('.'))
            # This will allow the program to continue but log the error
        except Exception as e:
            print(f"Error loading text configuration: {e}")
            print(f"Attempted to use file at: {file_path}")
            print(f"Current working directory: {os.getcwd()}")
            # List files in current directory to help debugging
            print("Files in current directory:", os.listdir('.'))

    def _resource_path(self, relative_path):
        """Get absolute path to resource, works for dev and PyInstaller"""
        try:
            # PyInstaller creates a temp folder and stores path in _MEIPASS
            base_path = sys._MEIPASS
            print(f"Running from PyInstaller bundle, base path: {base_path}")
        except Exception:
            base_path = os.path.abspath(".")
            print(f"Running from development environment, base path: {base_path}")

        full_path = os.path.join(base_path, relative_path)
        print(f"Looking for text config file at: {full_path}")
        return full_path

    def render(self, surface):
        """
        Render all text elements onto the given surface
        :param surface: Pygame surface to render text onto
        """
        for text, position, font_size in self.text_elements:
            # If this text uses a different font size than the default, create a new font object
            if font_size != self.default_font_size:
                font = pygame.font.SysFont('Arial', int(font_size))
            else:
                font = self.font

            # Render the text
            text_surface = font.render(text, True, (255, 255, 255))  # White text

            # Blit the text to the surface at the specified position
            surface.blit(text_surface, position)