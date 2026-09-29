import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

WEB_ASSETS = r"C:\Users\DELL\Desktop\school_stuff\EEE532\youtube\box2d\biro_game\web\assets"
WEB_DIST_ASSETS = r"C:\Users\DELL\Desktop\school_stuff\EEE532\youtube\box2d\biro_game\web_dist\assets"

os.makedirs(WEB_ASSETS, exist_ok=True)
os.makedirs(WEB_DIST_ASSETS, exist_ok=True)

FONT_BOLD = r"C:\Windows\Fonts\arialbd.ttf"
FONT_REG = r"C:\Windows\Fonts\arial.ttf"
FONT_CONSOLA = r"C:\Windows\Fonts\consolab.ttf"

def get_font(path, size):
    try:
        return ImageFont.truetype(path, size)
    except:
        return ImageFont.load_default()

# ------------------------------------------------------------------------------
# Refined Hand-Drawn Biro Ballpoint Pen Illustration
# ------------------------------------------------------------------------------
def draw_biro_pen(draw, cx, cy, length=120, angle_deg=0, is_player1=True):
    rad = np.deg2rad(angle_deg)
    cos_a, sin_a = np.cos(rad), np.sin(rad)
    perp_x, perp_y = -sin_a, cos_a
    
    half_l = length * 0.5
    thick = 9.0  # Hexagonal barrel thickness
    
    p_tip_x = cx + half_l * cos_a
    p_tip_y = cy + half_l * sin_a
    p_tail_x = cx - half_l * cos_a
    p_tail_y = cy - half_l * sin_a
    
    # 1. Barrel (clear transparent plastic with ink tube inside)
    # 4 corners of the barrel
    t_end = 22.0 # tip cone length
    c1 = (p_tail_x + perp_x * thick, p_tail_y + perp_y * thick)
    c2 = (p_tip_x - t_end * cos_a + perp_x * thick, p_tip_y - t_end * sin_a + perp_y * thick)
    c3 = (p_tip_x - t_end * cos_a - perp_x * thick, p_tip_y - t_end * sin_a - perp_y * thick)
    c4 = (p_tail_x - perp_x * thick, p_tail_y - perp_y * thick)
    
    # Fill barrel with white/cream transparent look
    draw.polygon([c1, c2, c3, c4], fill=(255, 255, 255, 255), outline=(20, 20, 20, 255), width=2)
    
    # Hexagonal facet lines
    draw.line([(p_tail_x + perp_x * 3, p_tail_y + perp_y * 3), 
               (p_tip_x - t_end * cos_a + perp_x * 3, p_tip_y - t_end * sin_a + perp_y * 3)], 
              fill=(160, 160, 160, 255), width=1)
    draw.line([(p_tail_x - perp_x * 3, p_tail_y - perp_y * 3), 
               (p_tip_x - t_end * cos_a - perp_x * 3, p_tip_y - t_end * sin_a - perp_y * 3)], 
              fill=(160, 160, 160, 255), width=1)
              
    # Inner ink reservoir tube (solid black ink)
    draw.line([(p_tail_x + 10 * cos_a, p_tail_y + 10 * sin_a), 
               (p_tip_x - t_end * cos_a, p_tip_y - t_end * sin_a)], 
              fill=(20, 20, 20, 255), width=3)
              
    # 2. Nose Cone & Brass Ballpoint
    cone_tip = (p_tip_x, p_tip_y)
    draw.polygon([c2, cone_tip, c3], fill=(240, 240, 240, 255), outline=(20, 20, 20, 255), width=2)
    # Brass point
    bp1 = (p_tip_x - 6 * cos_a + perp_x * 3, p_tip_y - 6 * sin_a + perp_y * 3)
    bp2 = (p_tip_x - 6 * cos_a - perp_x * 3, p_tip_y - 6 * sin_a - perp_y * 3)
    draw.polygon([bp1, cone_tip, bp2], fill=(20, 20, 20, 255))
    draw.ellipse([cone_tip[0]-2, cone_tip[1]-2, cone_tip[0]+2, cone_tip[1]+2], fill=(0, 0, 0, 255))
    
    # 3. Crown End-Plug on Tail
    draw.line([c1, c4], fill=(20, 20, 20, 255), width=3)
    plug_len = 10.0
    k1 = (p_tail_x - plug_len * cos_a + perp_x * (thick - 2), p_tail_y - plug_len * sin_a + perp_y * (thick - 2))
    k2 = (p_tail_x - plug_len * cos_a - perp_x * (thick - 2), p_tail_y - plug_len * sin_a - perp_y * (thick - 2))
    draw.polygon([(p_tail_x + perp_x * (thick - 2), p_tail_y + perp_y * (thick - 2)),
                  k1, k2,
                  (p_tail_x - perp_x * (thick - 2), p_tail_y - perp_y * (thick - 2))],
                 fill=(20, 20, 20, 255))
                 
    # 4. Optional Classic Pocket Clip on cap
    clip_start = (p_tail_x + 8 * cos_a + perp_x * (thick + 1), p_tail_y + 8 * sin_a + perp_y * (thick + 1))
    clip_mid = (p_tail_x + 28 * cos_a + perp_x * (thick + 4), p_tail_y + 28 * sin_a + perp_y * (thick + 4))
    clip_end = (p_tail_x + 36 * cos_a + perp_x * (thick + 1), p_tail_y + 36 * sin_a + perp_y * (thick + 1))
    draw.line([clip_start, clip_mid, clip_end], fill=(20, 20, 20, 255), width=2)


def create_desk_canvas(w=420, h=230):
    img = Image.new("RGBA", (w, h), (250, 246, 236, 255)) # Warm cream paper
    draw = ImageDraw.Draw(img, "RGBA")
    
    # Outer margin notebook rulings
    desk_rect = [18, 20, w - 18, h - 20]
    
    # Drop shadow under wooden desk blotter
    draw.rectangle([desk_rect[0]+4, desk_rect[1]+4, desk_rect[2]+4, desk_rect[3]+4], fill=(222, 214, 196, 255))
    # Desk wood tone
    draw.rectangle(desk_rect, fill=(244, 237, 222, 255), outline=(20, 20, 20, 255), width=2)
    
    # Subtle wood grain lines
    for gy in range(desk_rect[1] + 16, desk_rect[3] - 12, 26):
        draw.line([(desk_rect[0] + 6, gy), (desk_rect[2] - 6, gy)], fill=(225, 215, 196, 200), width=1)
        
    return img, draw, desk_rect


# ------------------------------------------------------------------------------
# 1. GENERATE MODE START PREVIEWS (1v1, 2v2, Battle Royale)
# ------------------------------------------------------------------------------
def generate_mode_previews():
    print("Generating Mode Start Previews...")
    f_title = get_font(FONT_BOLD, 14)
    f_sub = get_font(FONT_REG, 11)
    
    # --- 1v1 DUEL ---
    img, draw, d = create_desk_canvas()
    cx, cy = (d[0] + d[2]) // 2, (d[1] + d[3]) // 2 - 10
    
    # 2 Biros facing off head-to-head
    draw_biro_pen(draw, cx - 85, cy, length=110, angle_deg=0)
    draw_biro_pen(draw, cx + 85, cy, length=110, angle_deg=180)
    
    # Collision target spark in center
    draw.ellipse([cx - 16, cy - 16, cx + 16, cy + 16], outline=(20, 20, 20, 255), width=2)
    draw.text((cx - 11, cy - 8), "VS", fill=(20, 20, 20, 255), font=get_font(FONT_BOLD, 14))
    
    # Bottom label strip
    draw.rounded_rectangle([28, d[3] - 32, d[2] - 28, d[3] - 8], radius=4, fill=(20, 20, 20, 255))
    draw.text((38, d[3] - 29), "1v1 DUEL  •  2 Pens Head-to-Head", fill=(255, 255, 255, 255), font=f_title)
    
    for folder in [WEB_ASSETS, WEB_DIST_ASSETS]:
        img.save(os.path.join(folder, "preview_mode_1v1.png"))
        
    # --- 2v2 TEAMS ---
    img, draw, d = create_desk_canvas()
    cx, cy = (d[0] + d[2]) // 2, (d[1] + d[3]) // 2 - 10
    
    # 4 Pens in 2v2 formation
    draw_biro_pen(draw, cx - 90, cy - 38, length=95, angle_deg=0)
    draw_biro_pen(draw, cx - 90, cy + 38, length=95, angle_deg=0)
    
    draw_biro_pen(draw, cx + 90, cy - 38, length=95, angle_deg=180)
    draw_biro_pen(draw, cx + 90, cy + 38, length=95, angle_deg=180)
    
    draw.ellipse([cx - 16, cy - 16, cx + 16, cy + 16], outline=(20, 20, 20, 255), width=2)
    draw.text((cx - 11, cy - 8), "VS", fill=(20, 20, 20, 255), font=get_font(FONT_BOLD, 14))
    
    draw.rounded_rectangle([28, d[3] - 32, d[2] - 28, d[3] - 8], radius=4, fill=(20, 20, 20, 255))
    draw.text((38, d[3] - 29), "2v2 TEAMS  •  4 Pens Team Clash", fill=(255, 255, 255, 255), font=f_title)
    
    for folder in [WEB_ASSETS, WEB_DIST_ASSETS]:
        img.save(os.path.join(folder, "preview_mode_2v2.png"))

    # --- BATTLE ROYALE ---
    img, draw, d = create_desk_canvas()
    cx, cy = (d[0] + d[2]) // 2, (d[1] + d[3]) // 2 - 10
    
    # 4 Pens in 4 corners facing center
    draw_biro_pen(draw, cx - 85, cy - 44, length=90, angle_deg=35)
    draw_biro_pen(draw, cx + 85, cy - 44, length=90, angle_deg=145)
    draw_biro_pen(draw, cx - 85, cy + 44, length=90, angle_deg=-35)
    draw_biro_pen(draw, cx + 85, cy + 44, length=90, angle_deg=-145)
    
    draw.ellipse([cx - 22, cy - 22, cx + 22, cy + 22], outline=(20, 20, 20, 255), width=2)
    draw.text((cx - 16, cy - 7), "RING", fill=(20, 20, 20, 255), font=get_font(FONT_BOLD, 11))
    
    draw.rounded_rectangle([28, d[3] - 32, d[2] - 28, d[3] - 8], radius=4, fill=(20, 20, 20, 255))
    draw.text((38, d[3] - 29), "BATTLE ROYALE  •  Free-For-All Ring", fill=(255, 255, 255, 255), font=f_title)
    
    for folder in [WEB_ASSETS, WEB_DIST_ASSETS]:
        img.save(os.path.join(folder, "preview_mode_royale.png"))


# ------------------------------------------------------------------------------
# 2. GENERATE STAGE / PEN ORIENTATION PREVIEWS
# ------------------------------------------------------------------------------
def generate_stage_previews():
    print("Generating Stage Axis Previews...")
    f_title = get_font(FONT_BOLD, 14)
    
    # --- HORIZONTAL AXIS ---
    img, draw, d = create_desk_canvas()
    cx, cy = (d[0] + d[2]) // 2, (d[1] + d[3]) // 2 - 10
    draw_biro_pen(draw, cx - 85, cy, length=110, angle_deg=0)
    draw_biro_pen(draw, cx + 85, cy, length=110, angle_deg=180)
    
    # Double horizontal arrow
    draw.line([(cx - 24, cy), (cx + 24, cy)], fill=(20, 20, 20, 255), width=2)
    draw.polygon([(cx - 24, cy), (cx - 16, cy - 4), (cx - 16, cy + 4)], fill=(20, 20, 20, 255))
    draw.polygon([(cx + 24, cy), (cx + 16, cy - 4), (cx + 16, cy + 4)], fill=(20, 20, 20, 255))
    
    draw.rounded_rectangle([28, d[3] - 32, d[2] - 28, d[3] - 8], radius=4, fill=(20, 20, 20, 255))
    draw.text((38, d[3] - 29), "HORIZONTAL START  •  Tip-to-Tip (Left / Right)", fill=(255, 255, 255, 255), font=f_title)
    
    for folder in [WEB_ASSETS, WEB_DIST_ASSETS]:
        img.save(os.path.join(folder, "preview_stage_horizontal.png"))

    # --- VERTICAL AXIS ---
    img, draw, d = create_desk_canvas()
    cx, cy = (d[0] + d[2]) // 2, (d[1] + d[3]) // 2 - 10
    draw_biro_pen(draw, cx - 65, cy, length=105, angle_deg=-90)
    draw_biro_pen(draw, cx + 65, cy, length=105, angle_deg=-90)
    
    # Double vertical arrow
    draw.line([(cx, cy - 25), (cx, cy + 25)], fill=(20, 20, 20, 255), width=2)
    draw.polygon([(cx, cy - 25), (cx - 4, cy - 17), (cx + 4, cy - 17)], fill=(20, 20, 20, 255))
    draw.polygon([(cx, cy + 25), (cx - 4, cy + 17), (cx + 4, cy + 17)], fill=(20, 20, 20, 255))
    
    draw.rounded_rectangle([28, d[3] - 32, d[2] - 28, d[3] - 8], radius=4, fill=(20, 20, 20, 255))
    draw.text((38, d[3] - 29), "VERTICAL START  •  Side-by-Side (Up / Down)", fill=(255, 255, 255, 255), font=f_title)
    
    for folder in [WEB_ASSETS, WEB_DIST_ASSETS]:
        img.save(os.path.join(folder, "preview_stage_vertical.png"))

    # --- CROSS AXIS ---
    img, draw, d = create_desk_canvas()
    cx, cy = (d[0] + d[2]) // 2, (d[1] + d[3]) // 2 - 10
    draw_biro_pen(draw, cx - 75, cy, length=105, angle_deg=0)
    draw_biro_pen(draw, cx + 75, cy, length=105, angle_deg=-90)
    
    draw.rounded_rectangle([28, d[3] - 32, d[2] - 28, d[3] - 8], radius=4, fill=(20, 20, 20, 255))
    draw.text((38, d[3] - 29), "CROSS START  •  Perpendicular Angle (✛)", fill=(255, 255, 255, 255), font=f_title)
    
    for folder in [WEB_ASSETS, WEB_DIST_ASSETS]:
        img.save(os.path.join(folder, "preview_stage_cross.png"))


# ------------------------------------------------------------------------------
# 3. GENERATE TABLE ARENA PREVIEWS
# ------------------------------------------------------------------------------
def generate_table_previews():
    print("Generating Table Type Previews...")
    f_title = get_font(FONT_BOLD, 14)
    
    # --- OPEN DESK ---
    img, draw, d = create_desk_canvas()
    cx, cy = (d[0] + d[2]) // 2, (d[1] + d[3]) // 2 - 10
    
    # Dashed drop indicators
    for x in range(d[0], d[2], 14):
        draw.line([(x, d[1] - 5), (x + 7, d[1] - 5)], fill=(20, 20, 20, 160), width=2)
        draw.line([(x, d[3] + 5), (x + 7, d[3] + 5)], fill=(20, 20, 20, 160), width=2)
    for y in range(d[1], d[3], 14):
        draw.line([(d[0] - 5, y), (d[0] - 5, y + 7)], fill=(20, 20, 20, 160), width=2)
        draw.line([(d[2] + 5, y), (d[2] + 5, y + 7)], fill=(20, 20, 20, 160), width=2)
        
    draw_biro_pen(draw, cx - 70, cy, length=90, angle_deg=0)
    draw_biro_pen(draw, cx + 70, cy, length=90, angle_deg=180)
    
    draw.rounded_rectangle([28, d[3] - 32, d[2] - 28, d[3] - 8], radius=4, fill=(20, 20, 20, 255))
    draw.text((38, d[3] - 29), "OPEN DESK  •  All 4 Edges Open (High Danger)", fill=(255, 255, 255, 255), font=f_title)
    
    for folder in [WEB_ASSETS, WEB_DIST_ASSETS]:
        img.save(os.path.join(folder, "preview_table_open.png"))

    # --- FRONT PENCIL BARRIER ---
    img, draw, d = create_desk_canvas()
    cx, cy = (d[0] + d[2]) // 2, (d[1] + d[3]) // 2 - 2
    
    # Solid wooden ridge along top
    draw.rectangle([d[0], d[1], d[2], d[1] + 16], fill=(40, 28, 18, 255), outline=(20, 20, 20, 255), width=2)
    draw.text((cx - 70, d[1] + 2), "TOP PENCIL GROOVE BARRIER", fill=(250, 245, 235, 240), font=get_font(FONT_BOLD, 10))
    
    draw_biro_pen(draw, cx - 70, cy, length=90, angle_deg=0)
    draw_biro_pen(draw, cx + 70, cy, length=90, angle_deg=180)
    
    draw.rounded_rectangle([28, d[3] - 32, d[2] - 28, d[3] - 8], radius=4, fill=(20, 20, 20, 255))
    draw.text((38, d[3] - 29), "FRONT BARRIER  •  Pencil Groove Blocks Top", fill=(255, 255, 255, 255), font=f_title)
    
    for folder in [WEB_ASSETS, WEB_DIST_ASSETS]:
        img.save(os.path.join(folder, "preview_table_barrier.png"))

    # --- DUAL RAILS ---
    img, draw, d = create_desk_canvas()
    cx, cy = (d[0] + d[2]) // 2, (d[1] + d[3]) // 2 - 10
    
    draw.rectangle([d[0], d[1], d[2], d[1] + 15], fill=(40, 28, 18, 255), outline=(20, 20, 20, 255), width=2)
    draw.rectangle([d[0], d[3] - 15, d[2], d[3]], fill=(40, 28, 18, 255), outline=(20, 20, 20, 255), width=2)
    
    draw_biro_pen(draw, cx - 70, cy, length=90, angle_deg=0)
    draw_biro_pen(draw, cx + 70, cy, length=90, angle_deg=180)
    
    draw.rounded_rectangle([28, d[3] - 42, d[2] - 28, d[3] - 18], radius=4, fill=(20, 20, 20, 255))
    draw.text((38, d[3] - 39), "DUAL RAILS  •  Top & Bottom Rebound Walls", fill=(255, 255, 255, 255), font=f_title)
    
    for folder in [WEB_ASSETS, WEB_DIST_ASSETS]:
        img.save(os.path.join(folder, "preview_table_rails.png"))


# ------------------------------------------------------------------------------
# 4. GENERATE CLEAN 2-PHONE PAIRING GIF (No Missing Unicode Glyphs)
# ------------------------------------------------------------------------------
def generate_pairing_gif():
    print("Generating clean 2-Phone Pairing GIF...")
    GW, GH = 520, 320
    frames = []
    
    f_header = get_font(FONT_BOLD, 13)
    f_phone = get_font(FONT_BOLD, 14)
    f_body = get_font(FONT_BOLD, 12)
    f_sub = get_font(FONT_REG, 11)
    f_code = get_font(FONT_CONSOLA, 19)
    f_hint = get_font(FONT_BOLD, 12)
    
    steps = [
        {"step": 1, "p1": "HOST_TAP", "p2": "IDLE", "hint": "Step 1: Host opens online lobby and taps [HOST A DESK]"},
        {"step": 2, "p1": "CODE_READY", "p2": "IDLE", "hint": "Step 2: Room code & direct join link created instantly"},
        {"step": 3, "p1": "CODE_READY", "p2": "GUEST_JOIN", "hint": "Step 3: Friend clicks link or enters code '7K2M9'"},
        {"step": 4, "p1": "CONNECTED", "p2": "CONNECTED", "hint": "Step 4: DIRECT P2P CONNECTED! Both ready to flick!"}
    ]
    
    for s in steps:
        frame = Image.new("RGBA", (GW, GH), (250, 246, 236, 255))
        draw = ImageDraw.Draw(frame, "RGBA")
        
        # Black top header
        draw.rectangle([0, 0, GW, 34], fill=(20, 20, 20, 255))
        draw.text((16, 9), "P2P DESK MULTIPLAYER  •  HOW TO CONNECT (100% FREE)", fill=(255, 255, 255, 255), font=f_header)
        
        # Phones side-by-side
        pw, ph = 205, 225
        p1_x, p1_y = 35, 48
        p2_x, p2_y = 280, 48
        
        for px, py, title in [(p1_x, p1_y, "PHONE 1 (HOST)"), (p2_x, p2_y, "PHONE 2 (GUEST)")]:
            # Bezel shadow
            draw.rounded_rectangle([px+3, py+4, px+pw+3, py+ph+4], radius=14, fill=(225, 218, 204, 255))
            # Black phone bezel
            draw.rounded_rectangle([px, py, px+pw, py+ph], radius=14, fill=(18, 18, 18, 255), outline=(50, 50, 50, 255), width=2)
            # Screen (Cream notebook paper)
            draw.rounded_rectangle([px+7, py+8, px+pw-7, py+ph-8], radius=8, fill=(248, 244, 234, 255))
            # Title bar inside phone
            draw.text((px + 14, py + 12), title, fill=(20, 20, 20, 255), font=f_phone)
            draw.line([(px+10, py+32), (px+pw-10, py+32)], fill=(210, 200, 185, 255), width=1)
            
        # Draw Phone 1
        p1_state = s["p1"]
        if p1_state == "HOST_TAP":
            draw.rounded_rectangle([p1_x+16, p1_y+58, p1_x+pw-16, p1_y+102], radius=6, fill=(20, 20, 20, 255))
            draw.text((p1_x+32, p1_y+72), "TAP: HOST DESK", fill=(255, 255, 255, 255), font=f_body)
            draw.text((p1_x+25, p1_y+130), "Starting WebRTC P2P...", fill=(80, 80, 80, 255), font=f_sub)
        elif p1_state in ("CODE_READY", "GUEST_JOIN"):
            draw.text((p1_x+18, p1_y+42), "ROOM CODE:", fill=(20, 20, 20, 255), font=f_sub)
            draw.rounded_rectangle([p1_x+16, p1_y+58, p1_x+pw-16, p1_y+98], radius=6, fill=(255, 255, 255, 255), outline=(20, 20, 20, 255), width=2)
            draw.text((p1_x+48, p1_y+68), "7 K 2 M 9", fill=(20, 20, 20, 255), font=f_code)
            
            draw.rounded_rectangle([p1_x+16, p1_y+115, p1_x+pw-16, p1_y+148], radius=4, fill=(20, 20, 20, 255))
            draw.text((p1_x+35, p1_y+125), "SHARE DESK LINK", fill=(255, 255, 255, 255), font=f_body)
            draw.text((p1_x+32, p1_y+165), "Waiting for friend...", fill=(100, 100, 100, 255), font=f_sub)
        elif p1_state == "CONNECTED":
            draw.rounded_rectangle([p1_x+14, p1_y+48, p1_x+pw-14, p1_y+94], radius=6, fill=(20, 20, 20, 255))
            draw.text((p1_x+26, p1_y+58), "CONNECTED!", fill=(255, 255, 255, 255), font=f_phone)
            draw.text((p1_x+26, p1_y+76), "Guest Joined Desk", fill=(235, 235, 235, 255), font=f_sub)
            draw_biro_pen(draw, p1_x + pw//2, p1_y + 140, length=75, angle_deg=0)
            draw.text((p1_x+42, p1_y+175), "DESK READY!", fill=(20, 20, 20, 255), font=f_body)

        # Draw Phone 2
        p2_state = s["p2"]
        if p2_state == "IDLE":
            draw.rounded_rectangle([p2_x+16, p2_y+58, p2_x+pw-16, p2_y+102], radius=6, fill=(255, 255, 255, 255), outline=(20, 20, 20, 255), width=2)
            draw.text((p2_x+42, p2_y+72), "JOIN A DESK", fill=(20, 20, 20, 255), font=f_body)
            draw.text((p2_x+24, p2_y+130), "Click shared link or enter code", fill=(100, 100, 100, 255), font=f_sub)
        elif p2_state == "GUEST_JOIN":
            draw.text((p2_x+18, p2_y+42), "ENTER CODE:", fill=(20, 20, 20, 255), font=f_sub)
            draw.rounded_rectangle([p2_x+16, p2_y+58, p2_x+pw-16, p2_y+98], radius=6, fill=(255, 255, 255, 255), outline=(20, 20, 20, 255), width=2)
            draw.text((p2_x+48, p2_y+68), "7 K 2 M 9", fill=(20, 20, 20, 255), font=f_code)
            
            draw.rounded_rectangle([p2_x+16, p2_y+115, p2_x+pw-16, p2_y+148], radius=4, fill=(20, 20, 20, 255))
            draw.text((p2_x+48, p2_y+125), "TAP: JOIN NOW", fill=(255, 255, 255, 255), font=f_body)
            draw.text((p2_x+45, p2_y+165), "Connecting P2P...", fill=(100, 100, 100, 255), font=f_sub)
        elif p2_state == "CONNECTED":
            draw.rounded_rectangle([p2_x+14, p2_y+48, p2_x+pw-14, p2_y+94], radius=6, fill=(20, 20, 20, 255))
            draw.text((p2_x+26, p2_y+58), "CONNECTED!", fill=(255, 255, 255, 255), font=f_phone)
            draw.text((p2_x+26, p2_y+76), "Host Desk Ready", fill=(235, 235, 235, 255), font=f_sub)
            draw_biro_pen(draw, p2_x + pw//2, p2_y + 140, length=75, angle_deg=180)
            draw.text((p2_x+42, p2_y+175), "DESK READY!", fill=(20, 20, 20, 255), font=f_body)

        # Bottom hint bar
        draw.rectangle([0, GH - 34, GW, GH], fill=(238, 232, 218, 255))
        draw.line([(0, GH - 34), (GW, GH - 34)], fill=(20, 20, 20, 255), width=2)
        draw.text((16, GH - 25), s["hint"], fill=(20, 20, 20, 255), font=f_hint)

        # Quantize to 64 colors for tiny size
        pal_frame = frame.convert("RGB").quantize(colors=64)
        frames.append(pal_frame)

    durations = [1200, 1200, 1300, 2400]
    out_gif = os.path.join(WEB_ASSETS, "pairing_tutorial.gif")
    frames[0].save(out_gif, save_all=True, append_images=frames[1:], duration=durations, loop=0, optimize=True)
    frames[0].save(os.path.join(WEB_DIST_ASSETS, "pairing_tutorial.gif"), save_all=True, append_images=frames[1:], duration=durations, loop=0, optimize=True)
    print(f"Clean Pairing Tutorial GIF generated! Size: {os.path.getsize(out_gif)/1024:.1f} KB")


if __name__ == "__main__":
    generate_mode_previews()
    generate_stage_previews()
    generate_table_previews()
    generate_pairing_gif()
    print("ALL REFINED PREVIEWS AND GIF GENERATED!")
