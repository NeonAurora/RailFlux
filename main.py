# Import necessary modules
import pygame
import sys
from canvas import Canvas
from track import Track
from train import Train
from timer import Timer
from settings import SCREEN_WIDTH, SCREEN_HEIGHT, BG_COLOR, TRACK_COLOR, TRAIN_COLOR, GRID_SIZE, CELL_SIZE, SPEED, FPS

# Main game loop
def main():
    # Initialize classes
    canvas = Canvas(SCREEN_WIDTH, SCREEN_HEIGHT, BG_COLOR)
    track = Track(GRID_SIZE, CELL_SIZE, TRACK_COLOR)
    train = Train(CELL_SIZE, CELL_SIZE * 2, TRAIN_COLOR, SPEED, [0, 0], "right")
    timer = Timer(FPS)

    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()

        # Clear canvas
        canvas.clear()

        # Draw track
        track.draw(canvas.screen)

        # Move and draw train
        train.move(SCREEN_WIDTH, SCREEN_HEIGHT, CELL_SIZE)
        train.draw(canvas.screen)

        # Update display
        canvas.update()

        # Cap the frame rate
        timer.tick()

# Run the main function
if __name__ == "__main__":
    main()
