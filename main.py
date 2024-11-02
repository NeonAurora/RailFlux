# main.py
import pygame
import sys
from canvas import Canvas
from timer import Timer
from settings import WINDOW_WIDTH, WINDOW_HEIGHT, CANVAS_WIDTH, CANVAS_HEIGHT, BG_COLOR, HEADER_HEIGHT, FOOTER_HEIGHT, FPS

def main():
    pygame.init()
    screen = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
    pygame.display.set_caption("Track Simulation with Header and Footer")

    canvas = Canvas(CANVAS_WIDTH, CANVAS_HEIGHT, BG_COLOR, HEADER_HEIGHT)
    timer = Timer(FPS)

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

        # Update and draw the canvas area within the window
        canvas.update(screen)

        # Update display
        pygame.display.flip()

        # Cap the frame rate
        timer.tick()

if __name__ == "__main__":
    main()
