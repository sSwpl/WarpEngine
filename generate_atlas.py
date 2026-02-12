from PIL import Image, ImageDraw
import random

# Increase to 256x256 (4x4 grid of 64px)
IMG_SIZE = 256
CELL_SIZE = 64

atlas = Image.new('RGBA', (IMG_SIZE, IMG_SIZE), (0, 0, 0, 0))
pixels = atlas.load()

def draw_player(draw, x, y):
    # White Knight
    draw.rectangle([x+20, y+10, x+44, y+50], fill=(200, 200, 200, 255)) 
    draw.rectangle([x+28, y+2, x+36, y+10], fill=(200, 200, 200, 255)) # Head
    draw.rectangle([x+10, y+20, x+20, y+40], fill=(150, 150, 150, 255)) # Shield
    draw.line([x+44, y+20, x+60, y+20], fill=(255, 255, 255, 255), width=3) # Sword

def draw_blob(draw, x, y):
    # Green Blob (now white base for tinting)
    draw.ellipse([x+10, y+10, x+54, y+54], fill=(255, 255, 255, 255))
    draw.rectangle([x+20, y+30, x+44, y+40], fill=(0, 0, 0, 255)) # Mouth
    draw.ellipse([x+15, y+20, x+25, y+30], fill=(0, 0, 0, 255)) # Eye L
    draw.ellipse([x+39, y+20, x+49, y+30], fill=(0, 0, 0, 255)) # Eye R

def draw_skeleton(draw, x, y):
    # Skeleton (White base)
    draw.rectangle([x+24, y+10, x+40, y+26], fill=(240, 240, 240, 255)) # Skull
    draw.line([x+32, y+26, x+32, y+46], fill=(240, 240, 240, 255), width=3) # Spine
    draw.line([x+20, y+30, x+44, y+30], fill=(240, 240, 240, 255), width=2) # Arms
    draw.line([x+24, y+46, x+24, y+60], fill=(240, 240, 240, 255), width=2) # Leg L
    draw.line([x+40, y+46, x+40, y+60], fill=(240, 240, 240, 255), width=2) # Leg R

def draw_crystal(draw, x, y):
    # Diamond Shape (White for Tinting)
    draw.polygon([(x+32, y+10), (x+54, y+32), (x+32, y+54), (x+10, y+32)], fill=(255, 255, 255, 255)) 

def draw_green_gem(draw, x, y):
    # Heart/Cross shape
    draw.rectangle([x+28, y+10, x+36, y+54], fill=(255, 255, 255, 255))
    draw.rectangle([x+10, y+28, x+54, y+36], fill=(255, 255, 255, 255))

def draw_purple_gem(draw, x, y):
    # Lightning Bolt
    points = [(x+32, y+10), (x+20, y+32), (x+32, y+32), (x+20, y+54), (x+44, y+30), (x+32, y+30)]
    draw.polygon(points, fill=(255, 255, 255, 255))

draw = ImageDraw.Draw(atlas)

# Row 1
draw_player(draw, 0, 0)
draw_blob(draw, 64, 0)
draw_skeleton(draw, 128, 0)
draw_crystal(draw, 192, 0)

# Row 2 (Powerups)
draw_green_gem(draw, 0, 64)   # (0, 1)
draw_purple_gem(draw, 64, 64) # (1, 1)

atlas.save('assets/atlas.png')
print("Generated assets/atlas.png (256x256)")
