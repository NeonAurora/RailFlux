# Import Pygame
import pygame
import sys

# Initialize Pygame
pygame.init()

# Screen settings
SCREEN_WIDTH, SCREEN_HEIGHT = 600, 600
BG_COLOR = (30, 30, 30)  # Background color
TRACK_COLOR = (180, 180, 180)  # Track color
TRAIN_COLOR = (255, 50, 50)  # Train color

# Track settings
GRID_SIZE = 6  # 6x6 grid
CELL_SIZE = SCREEN_WIDTH // GRID_SIZE

# Train settings
TRAIN_WIDTH, TRAIN_HEIGHT = CELL_SIZE, CELL_SIZE * 2  # "1x2" train size
train_position = [0, 0]  # Starting position (top-left of the loop)
train_direction = "right"  # Initial direction of train

# Movement speed
SPEED = 2  # Pixels per frame

# Set up the display
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Simple 6x6 Loop Track with Moving Train")


# Function to draw the track grid
def draw_track():
    for row in range(GRID_SIZE):
        for col in range(GRID_SIZE):
            # Define cell boundaries
            cell_rect = pygame.Rect(col * CELL_SIZE, row * CELL_SIZE, CELL_SIZE, CELL_SIZE)
            # Draw the cell with track color
            pygame.draw.rect(screen, TRACK_COLOR, cell_rect, 2)  # 2 px border


# Function to move the train along the loop
def move_train():
    global train_position, train_direction

    if train_direction == "right":
        train_position[0] += SPEED
        if train_position[0] >= SCREEN_WIDTH - CELL_SIZE:
            train_direction = "down"
    elif train_direction == "down":
        train_position[1] += SPEED
        if train_position[1] >= SCREEN_HEIGHT - CELL_SIZE * 2:
            train_direction = "left"
    elif train_direction == "left":
        train_position[0] -= SPEED
        if train_position[0] <= 0:
            train_direction = "up"
    elif train_direction == "up":
        train_position[1] -= SPEED
        if train_position[1] <= 0:
            train_direction = "right"


# Main game loop
def main():
    clock = pygame.time.Clock()

    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()

        # Clear screen
        screen.fill(BG_COLOR)

        # Draw track grid
        draw_track()

        # Move train
        move_train()

        # Draw the train as a red rectangle
        train_rect = pygame.Rect(train_position[0], train_position[1], TRAIN_WIDTH, TRAIN_HEIGHT)
        pygame.draw.rect(screen, TRAIN_COLOR, train_rect)

        # Update display
        pygame.display.flip()

        # Cap the frame rate
        clock.tick(30)  # 30 frames per second


# Run the main function
if __name__ == "__main__":
    main()
