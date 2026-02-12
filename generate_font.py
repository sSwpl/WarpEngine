from PIL import Image, ImageDraw, ImageFont
import os

# Generate 256x256 bitmap font atlas
# 16x16 grid, each cell 16x16 pixels
# ASCII 32..127 (96 printable chars)

IMG_SIZE = 256
CELL = 16
COLS = 16
ROWS = 16

atlas = Image.new('RGBA', (IMG_SIZE, IMG_SIZE), (0, 0, 0, 0))
draw = ImageDraw.Draw(atlas)

# Try to use a monospace font, fallback to default
try:
    font = ImageFont.truetype("consola.ttf", 14)  # Windows Consolas
except:
    try:
        font = ImageFont.truetype("cour.ttf", 14)  # Courier
    except:
        font = ImageFont.load_default()

for i in range(96):
    ch = chr(32 + i)
    col = i % COLS
    row = i // COLS
    x = col * CELL + 2
    y = row * CELL + 1
    draw.text((x, y), ch, fill=(255, 255, 255, 255), font=font)

atlas.save('assets/font.png')
print(f"Generated assets/font.png ({IMG_SIZE}x{IMG_SIZE})")
