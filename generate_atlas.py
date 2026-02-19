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

def draw_bones(draw, x, y):
    # Skeleton bones (corpse)
    draw.ellipse([x+22, y+18, x+42, y+34], fill=(200, 200, 200, 255)) # Skull
    draw.ellipse([x+28, y+22, x+33, y+28], fill=(0, 0, 0, 255)) # Eye L
    draw.ellipse([x+35, y+22, x+40, y+28], fill=(0, 0, 0, 255)) # Eye R
    draw.line([x+18, y+38, x+46, y+48], fill=(200, 200, 200, 255), width=3) # Bone 1
    draw.line([x+18, y+48, x+46, y+38], fill=(200, 200, 200, 255), width=3) # Bone 2

def draw_slime(draw, x, y):
    # Blob slime puddle (corpse)
    draw.ellipse([x+8, y+30, x+56, y+52], fill=(180, 220, 180, 200))
    draw.ellipse([x+14, y+34, x+50, y+48], fill=(140, 200, 140, 220))
    draw.ellipse([x+20, y+24, x+36, y+38], fill=(160, 210, 160, 180))

def draw_machinegun(draw, x, y):
    # Machine gun icon
    draw.rectangle([x+12, y+28, x+52, y+36], fill=(200, 200, 200, 255)) # Barrel
    draw.rectangle([x+8, y+24, x+28, y+40], fill=(180, 180, 180, 255))  # Body
    draw.rectangle([x+14, y+40, x+22, y+52], fill=(160, 160, 160, 255)) # Grip
    draw.rectangle([x+24, y+40, x+30, y+48], fill=(140, 140, 140, 255)) # Magazine
    draw.ellipse([x+48, y+26, x+56, y+38], fill=(255, 200, 50, 255))    # Muzzle flash

def draw_sword(draw, x, y):
    # Sword icon
    draw.line([x+10, y+54, x+50, y+10], fill=(220, 230, 255, 255), width=4)  # Blade
    draw.line([x+10, y+54, x+50, y+10], fill=(255, 255, 255, 255), width=2)  # Edge
    draw.rectangle([x+16, y+38, x+34, y+42], fill=(200, 170, 50, 255))       # Guard
    draw.line([x+10, y+54, x+14, y+60], fill=(140, 100, 40, 255), width=3)   # Handle

def draw_bazooka(draw, x, y):
    # Bazooka/rocket launcher icon
    draw.rectangle([x+8, y+26, x+52, y+38], fill=(100, 120, 100, 255))   # Tube
    draw.ellipse([x+4, y+24, x+14, y+40], fill=(80, 100, 80, 255))       # Back opening
    draw.rectangle([x+46, y+22, x+56, y+42], fill=(120, 140, 120, 255))  # Front
    draw.rectangle([x+20, y+38, x+28, y+52], fill=(100, 100, 100, 255))  # Grip
    draw.polygon([(x+56, y+28), (x+62, y+24), (x+62, y+40), (x+56, y+36)], fill=(255, 100, 50, 255)) # Rocket tip

def draw_sword_slash(draw, x, y):
    # Curved sword slash arc - crescent shape
    # Outer arc
    draw.arc([x+2, y+2, x+62, y+62], start=200, end=340, fill=(255, 255, 255, 255), width=6)
    draw.arc([x+6, y+6, x+58, y+58], start=210, end=330, fill=(255, 255, 255, 200), width=4)
    draw.arc([x+10, y+10, x+54, y+54], start=220, end=320, fill=(255, 255, 255, 150), width=3)
def draw_ground_tile_1(draw, x, y):
    # Dark grass tile with subtle texture
    draw.rectangle([x, y, x+63, y+63], fill=(25, 35, 20, 255))
    # Grass blades / dots
    import random
    rng = random.Random(42)
    for _ in range(12):
        px, py = rng.randint(x+2, x+61), rng.randint(y+2, y+61)
        c = rng.choice([(30, 45, 25, 255), (20, 30, 18, 255), (35, 50, 30, 255)])
        draw.rectangle([px, py, px+2, py+2], fill=c)
    # Subtle grid lines at edges
    draw.line([x, y, x+63, y], fill=(22, 32, 18, 255), width=1)
    draw.line([x, y, x, y+63], fill=(22, 32, 18, 255), width=1)

def draw_ground_tile_2(draw, x, y):
    # Slightly different ground variant
    draw.rectangle([x, y, x+63, y+63], fill=(22, 30, 22, 255))
    import random
    rng = random.Random(99)
    for _ in range(8):
        px, py = rng.randint(x+4, x+59), rng.randint(y+4, y+59)
        c = rng.choice([(28, 40, 22, 255), (18, 28, 16, 255)])
        draw.ellipse([px, py, px+3, py+3], fill=c)
    draw.line([x, y, x+63, y], fill=(20, 28, 18, 255), width=1)
    draw.line([x, y, x, y+63], fill=(20, 28, 18, 255), width=1)

draw = ImageDraw.Draw(atlas)

# Row 1
draw_player(draw, 0, 0)
draw_blob(draw, 64, 0)
draw_skeleton(draw, 128, 0)
draw_crystal(draw, 192, 0)

# Row 2 (Powerups + Corpses)
draw_green_gem(draw, 0, 64)   # (0, 1)
draw_purple_gem(draw, 64, 64) # (1, 1)
draw_bones(draw, 128, 64)     # (2, 1) - Skeleton corpse
draw_slime(draw, 192, 64)     # (3, 1) - Blob corpse

# Row 3 (Weapons + Slash)
draw_machinegun(draw, 0, 128)    # (0, 2) - Machine gun icon
draw_sword(draw, 64, 128)        # (1, 2) - Sword icon
draw_bazooka(draw, 128, 128)     # (2, 2) - Bazooka icon
draw_sword_slash(draw, 192, 128) # (3, 2) - Sword slash arc

# Row 4 (Ground tiles)
draw_ground_tile_1(draw, 0, 192)   # (0, 3) - Dark grass
draw_ground_tile_2(draw, 64, 192)  # (1, 3) - Grass variant

atlas.save('assets/atlas.png')
print("Generated assets/atlas.png (256x256)")

