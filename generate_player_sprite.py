from PIL import Image

# Create a 64x64 pixel art warrior character
img = Image.new('RGBA', (64, 64), (0, 0, 0, 0))
pixels = img.load()

# Color palette
SKIN = (255, 220, 180)
SKIN_DARK = (200, 160, 130)
ARMOR = (60, 80, 120)
ARMOR_LIGHT = (80, 110, 160)
ARMOR_DARK = (40, 60, 90)
METAL = (160, 170, 180)
SWORD = (200, 200, 220)
CAPE = (140, 40, 50)

# Draw from bottom to top (back to front layering)

# Cape (back layer)
for y in range(28, 50):
    for x in range(18, 22):
        pixels[x, y] = CAPE
    for x in range(42, 46):
        pixels[x, y] = CAPE

# Legs (armor)
for y in range(42, 58):
    for x in range(24, 28):
        pixels[x, y] = ARMOR
    for x in range(36, 40):
        pixels[x, y] = ARMOR

# Body (armor chest)
for y in range(28, 42):
    for x in range(20, 44):
        if x < 22 or x >= 42 or y < 30 or y >= 40:
            pixels[x, y] = ARMOR_DARK
        else:
            pixels[x, y] = ARMOR

# Arms
for y in range(30, 42):
    for x in range(16, 20):
        pixels[x, y] = ARMOR_LIGHT  # Left arm
    for x in range(44, 48):
        pixels[x, y] = ARMOR_LIGHT  # Right arm

# Sword (right side)
for y in range(18, 38):
    for x in range(50, 54):
        if y < 22:
            pixels[x, y] = METAL  # Hilt
        else:
            pixels[x, y] = SWORD  # Blade

# Head
for y in range(16, 28):
    for x in range(26, 38):
        pixels[x, y] = SKIN

# Helmet
for y in range(14, 20):
    for x in range(24, 40):
        pixels[x, y] = METAL

# Face details (eyes)
pixels[29, 21] = (40, 40, 40)
pixels[30, 21] = (40, 40, 40)
pixels[33, 21] = (40, 40, 40)
pixels[34, 21] = (40, 40, 40)

# Armor details (chest plate lines)
for y in range(32, 38):
    pixels[31, y] = ARMOR_LIGHT
    pixels[32, y] = ARMOR_LIGHT

# Belt
for x in range(22, 42):
    pixels[x, 41] = ARMOR_DARK

img.save('player.png')
print("Generated player.png")
