# Window and Canvas Settings
WINDOW_WIDTH = 1920
WINDOW_HEIGHT = 1020
HEADER_HEIGHT = 100
FOOTER_HEIGHT = 10

# The canvas area will be in between the header and footer
CANVAS_WIDTH = WINDOW_WIDTH
CANVAS_HEIGHT = WINDOW_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT

# Background and Color Settings
BG_COLOR = (30, 30, 30)  # Background color
TRACK_COLOR = (234, 224, 200)  # Pearl White for the grid color
TRAIN_COLOR = (255, 50, 50)  # Train color
TRAIN2_COLOR = (0, 0, 255)  # Train 2 color (blue)
TRAIN3_COLOR = (0, 255, 0)  # Train 3 color (green)

# LC_GATE Settings
LC_GATE_COLOR = (100, 100, 100)  # Yellow color for LC gates
LC_GATE_THICKNESS = 3  # Line thickness for LC gates
LC_GATE_CIRCLE_RADIUS = 5  # Circle radius in pixels for LC gates

# Grid Settings
GRID_ROWS = 200  # Number of rows in the grid
GRID_COLUMNS = 250  # Number of columns in the grid
CELL_SIZE = CANVAS_WIDTH / GRID_COLUMNS

# Train Settings
SPEED = 2  # Pixels per frame
TRAIN_TRANSPARENT = False  # Toggle for train transparency (True for transparent)

# Timer Settings
FPS = 30  # Frames per second

# New variables for grid customization
GRID_BORDER_COLOR = (255, 255, 255, 50)  # Grid border color with 50% transparency
GRID_BORDER_THICKNESS = 1  # Set border thickness (1 pixel)
SECOND_PRIORITY_COLOR = (255, 0, 0, 128)  # Second priority color (red with 50% transparency)
