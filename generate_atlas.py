from PIL import Image, ImageDraw

# Create 256x256 atlas (2x2 grid of 64x64 sprites? No, 2x2 grid of 128x128 slots? Or 4x4 grid of 64x64?)
# Let's stick to 2x2 grid where each cell is 64x64. So image size 128x128.
# (0,0) Player
# (1,0) Blob
# (0,1) Skeleton
# (1,1) Crystal

IMG_SIZE = 128
CELL_SIZE = 64

atlas = Image.new('RGBA', (IMG_SIZE, IMG_SIZE), (0, 0, 0, 0))
pixels = atlas.load()

# --- 1. PLAYER (Top-Left 0,0) ---
# Copied from previous script logic, simplified
def draw_player(base_x, base_y):
    SKIN = (255, 220, 180)
    ARMOR = (60, 80, 120)
    ARMOR_DARK = (40, 60, 90)
    CAPE = (140, 40, 50)
    METAL = (160, 170, 180)
    
    # Cape
    for y in range(28, 50):
        for x in range(18, 46):
            if x < 22 or x >= 42:
                pixels[base_x+x, base_y+y] = CAPE
    # Body
    for y in range(28, 58):
        for x in range(24, 40):
            pixels[base_x+x, base_y+y] = ARMOR
    # Head
    for y in range(16, 28):
        for x in range(26, 38):
            pixels[base_x+x, base_y+y] = SKIN
    # Helmet
    for y in range(14, 20):
        for x in range(24, 40):
            pixels[base_x+x, base_y+y] = METAL
    # Face
    pixels[base_x+29, base_y+21] = (40, 40, 40)
    pixels[base_x+33, base_y+21] = (40, 40, 40)

draw_player(0, 0)

# --- 2. BLOB (Top-Right 1,0) ---
def draw_blob(base_x, base_y):
    COLOR = (50, 200, 50)
    COLOR_DARK = (20, 140, 20)
    # Circle-ish shape
    for y in range(64):
        for x in range(64):
            dx = x - 32
            dy = y - 32
            dist = (dx*dx + dy*dy)**0.5
            if dist < 24:
                pixels[base_x+x, base_y+y] = COLOR
            elif dist < 26:
                pixels[base_x+x, base_y+y] = COLOR_DARK
    # Eyes
    pixels[base_x+24, base_y+24] = (255, 255, 255)
    pixels[base_x+40, base_y+24] = (255, 255, 255)
    pixels[base_x+24, base_y+25] = (0, 0, 0)
    pixels[base_x+40, base_y+25] = (0, 0, 0)

draw_blob(64, 0)

# --- 3. SKELETON (Bottom-Left 0,1) ---
def draw_skeleton(base_x, base_y):
    BONE = (220, 220, 220)
    for y in range(16, 56):
        # Spine
        pixels[base_x+32, base_y+y] = BONE
        pixels[base_x+31, base_y+y] = BONE
    # Ribs
    for y in range(30, 40, 3):
        for x in range(24, 40):
            pixels[base_x+x, base_y+y] = BONE
    # Head
    for y in range(16, 28):
        for x in range(26, 38):
            pixels[base_x+x, base_y+y] = BONE
    # Eyes
    pixels[base_x+29, base_y+22] = (0, 0, 0)
    pixels[base_x+34, base_y+22] = (0, 0, 0)

draw_skeleton(0, 64)

# --- 4. CRYSTAL (Bottom-Right 1,1) ---
def draw_crystal(base_x, base_y):
    BLUE = (100, 200, 255)
    WHITE = (255, 255, 255)
    # Diamond shape
    for y in range(16, 48):
        for x in range(16, 48):
            dx = abs(x - 32)
            dy = abs(y - 32)
            if dx + dy < 16:
                pixels[base_x+x, base_y+y] = BLUE
    # Shine
    pixels[base_x+32, base_y+24] = WHITE
    pixels[base_x+32, base_y+25] = WHITE

draw_crystal(64, 64)

atlas.save('assets/atlas.png')
print("Generated assets/atlas.png (128x128)")
