from PIL import Image

# Create a 64x64 image with a simple player sprite design
img = Image.new('RGBA', (64, 64), (0, 0, 0, 0))
pixels = img.load()

# Simple player sprite - a colored square with a border
for y in range(64):
    for x in range(64):
        # Border (dark)
        if x < 4 or x >= 60 or y < 4 or y >= 60:
            pixels[x, y] = (40, 40, 40, 255)
        # Inner body (blue-ish color for player)
        elif 8 <= x < 56 and 8 <= y < 56:
            pixels[x, y] = (60, 120, 180, 255)
        # Middle border (lighter)
        else:
            pixels[x, y] = (80, 140, 200, 255)

# Add a simple "face" or detail
for y in range(20, 28):
    for x in range(24, 32):
        pixels[x, y] = (255, 220, 180, 255)  # light spot

for y in range(28, 36):
    for x in range(20, 44):
        pixels[x, y] = (255, 220, 180, 255)  # light spot

img.save('player.png')
print("Generated player.png")
