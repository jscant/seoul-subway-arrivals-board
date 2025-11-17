from PIL import Image, ImageDraw, ImageFont
import subprocess
import os

GLYPHS = {
    "강": "gang", "남": "nam", "고": "go", "석": "sok", "터": "to", "미": "mi", "널": "nol",
    "서": "seo", "초": "cho", "부": "bu"}
SIZE = 64
FONT = "include/NanumGothicCoding-Bold.ttf"

font = ImageFont.truetype(FONT, SIZE)

for ko, en in GLYPHS.items():
    # Estimate bounding box
    w, h = font.getbbox(ko)[2:]
    w = max(w, SIZE)
    h = max(h, SIZE)

    img = Image.new("1", (w, h), 1)   # white background
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), ko, font=font, fill=0)

    png_fname = f"include/{en}.png"
    img.save(png_fname)
    subprocess.run(["convert", png_fname, f"include/{en}.xbm"])
    
    with open(f"include/{en}.xbm", "r") as f:
        xbm_data = f.read()
        
    xbm_data = xbm_data.replace("static char", "const uint8_t")
    
    with open(f"include/{en}.xbm", "w") as f:
        f.write(xbm_data)
        
    os.remove(png_fname)