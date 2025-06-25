# main.py
import pygame
import sys
import firebase_setup  # Import firebase_setup to ensure Firebase is initialized before anything else
from firebase_admin import db  # Import db from Firebase Admin SDK
from canvas import Canvas
from timer import Timer
from settings import (
    WINDOW_WIDTH, WINDOW_HEIGHT, CANVAS_WIDTH, CANVAS_HEIGHT, BG_COLOR,
    HEADER_HEIGHT, FOOTER_HEIGHT, FPS, TRACK_COLOR, TRAIN_COLOR, TRAIN2_COLOR,
    GRID_ROWS, GRID_COLUMNS, CELL_SIZE, GRID_BORDER_COLOR, GRID_BORDER_THICKNESS,
    SECOND_PRIORITY_COLOR, SPEED, TRAIN_TRANSPARENT
)


def push_metadata_to_firebase():
    # Firebase reference to the Metadata path
    metadata_ref = db.reference('/Metadata')

    # Define the metadata dictionary
    metadata = {
        "WindowSettings": {
            "WINDOW_WIDTH": WINDOW_WIDTH,
            "WINDOW_HEIGHT": WINDOW_HEIGHT,
            "HEADER_HEIGHT": HEADER_HEIGHT,
            "FOOTER_HEIGHT": FOOTER_HEIGHT,
            "CANVAS_WIDTH": CANVAS_WIDTH,
            "CANVAS_HEIGHT": CANVAS_HEIGHT
        },
        "BackgroundAndColorSettings": {
            "BG_COLOR": BG_COLOR,
            "TRACK_COLOR": TRACK_COLOR,
            "TRAIN_COLOR": TRAIN_COLOR,
            "TRAIN2_COLOR": TRAIN2_COLOR,
            "GRID_BORDER_COLOR": GRID_BORDER_COLOR,
            "GRID_BORDER_THICKNESS": GRID_BORDER_THICKNESS,
            "SECOND_PRIORITY_COLOR": SECOND_PRIORITY_COLOR
        },
        "GridSettings": {
            "GRID_ROWS": GRID_ROWS,
            "GRID_COLUMNS": GRID_COLUMNS,
            "CELL_SIZE": CELL_SIZE
        },
        "TrainSettings": {
            "SPEED": SPEED,
            "TRAIN_TRANSPARENT": TRAIN_TRANSPARENT
        },
        "TimerSettings": {
            "FPS": FPS
        }
    }

    # Write metadata to Firebase
    metadata_ref.set(metadata)
    print("Metadata successfully pushed to Firebase.")


def main():
    # Initialize pygame
    pygame.init()
    screen = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
    pygame.display.set_caption("Track Simulation")

    # Initialize font for header text
    header_font = pygame.font.SysFont('Arial', 48, bold=True)  # Use SysFont if you don't have a custom font

    # Push metadata to Firebase when initializing the application
    push_metadata_to_firebase()

    # Initialize the canvas and timer
    canvas = Canvas(CANVAS_WIDTH, CANVAS_HEIGHT, BG_COLOR, HEADER_HEIGHT)
    timer = Timer(FPS)

    # Main loop
    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()

        # Clear the entire window
        screen.fill(BG_COLOR)

        # Draw header and footer sections
        header_rect = pygame.Rect(0, 0, WINDOW_WIDTH, HEADER_HEIGHT)
        footer_rect = pygame.Rect(0, WINDOW_HEIGHT - FOOTER_HEIGHT, WINDOW_WIDTH, FOOTER_HEIGHT)
        pygame.draw.rect(screen, (50, 50, 50), header_rect)  # Dark grey for header
        pygame.draw.rect(screen, (50, 50, 50), footer_rect)  # Dark grey for footer

        # Render "CHUADANGA" text
        header_text = header_font.render("CHUADANGA", True, (255, 255, 255))  # White text
        text_rect = header_text.get_rect(center=(WINDOW_WIDTH // 2, HEADER_HEIGHT // 2))
        screen.blit(header_text, text_rect)

        # Update and draw the canvas area within the window
        canvas.update(screen)

        # Update display
        pygame.display.flip()

        # Cap the frame rate
        timer.tick()


if __name__ == "__main__":
    main()