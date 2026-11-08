import bpy
import os
import tempfile
import numpy as np
import json
from bpy_extras.object_utils import world_to_camera_view

# ==========================================
# CONFIGURATION
# ==========================================
DEFAULT_FRAMES = 128     # Fallback if the camera lacks the "Frames" property
BASE_WIDTH = 800         # Your plugin window width (1x scale)
BASE_HEIGHT = 1080        # Your plugin window height (1x scale)
BACKGROUND_CAMERA_NAME = "Camera"  # Camera used to render Main.png
RETINA_SCALE = 1         # Render at 2x resolution for High-DPI displays
OUTPUT_DIR = r"I:\dev\synth-guitar\src\Rendered UI"
# ==========================================

scene = bpy.context.scene
scene.render.film_transparent = True
scene.render.image_settings.color_mode = 'RGBA'
scene.render.image_settings.file_format = 'PNG'

background_camera = bpy.data.objects.get(BACKGROUND_CAMERA_NAME)
if not background_camera or background_camera.type != 'CAMERA' or background_camera.data.type != 'ORTHO':
    raise Exception(f"Background camera '{BACKGROUND_CAMERA_NAME}' must be an orthographic camera.")

# Blender's orthographic scale spans the larger render dimension with AUTO sensor fit.
scene.render.resolution_x = BASE_WIDTH
scene.render.resolution_y = BASE_HEIGHT
scene.render.resolution_percentage = 100
scene.render.pixel_aspect_x = 1.0
scene.render.pixel_aspect_y = 1.0

tmp_dir = r"I:\BlendTemp"
os.makedirs(tmp_dir, exist_ok=True)
layout_data = {}

camera_collection = bpy.data.collections.get("UI_Cameras")
if not camera_collection:
    raise Exception("Please create a collection named 'UI_Cameras' and put your element cameras inside it.")

print("Starting Batch Render & Coordinate Extraction...")

for obj in camera_collection.objects:
    if obj.type != 'CAMERA' or obj.data.type != 'ORTHO' or obj.hide_render:
        continue
    if obj == background_camera:
        continue
        
    cam_name = obj.name
    
    # Camera Data properties take precedence over Object properties.
    frames = int(obj.data.get("Frames", obj.get("Frames", DEFAULT_FRAMES)))
    startFrame = int(obj.data.get("StartFrame", obj.get("StartFrame", 1)))
    if frames < 1:
        raise ValueError(f"{cam_name}: Frames must be at least 1.")
    
    print(f"\nProcessing {cam_name} ({frames} frames)...")
    
    # 1. CALCULATE RESOLUTION & COORDINATES
    scene.frame_set(startFrame)
    scene.render.resolution_x = BASE_WIDTH
    scene.render.resolution_y = BASE_HEIGHT
    depsgraph = bpy.context.evaluated_depsgraph_get()
    main_camera = background_camera.evaluated_get(depsgraph)
    element_camera = obj.evaluated_get(depsgraph)
    main_rotation = main_camera.matrix_world.to_quaternion()
    element_rotation = element_camera.matrix_world.to_quaternion()
    if main_rotation.rotation_difference(element_rotation).angle > 0.0001:
        raise Exception(f"{cam_name} must have the same orientation as {BACKGROUND_CAMERA_NAME}.")
    if element_camera.data.shift_x != 0.0 or element_camera.data.shift_y != 0.0:
        raise Exception(f"{cam_name} must have zero camera shift for a centered crop.")

    main_frame = main_camera.data.view_frame(scene=scene)
    pixels_per_unit_x = BASE_WIDTH / (max(v.x for v in main_frame) - min(v.x for v in main_frame))
    pixels_per_unit_y = BASE_HEIGHT / (max(v.y for v in main_frame) - min(v.y for v in main_frame))
    center = world_to_camera_view(scene, main_camera, element_camera.matrix_world.translation)
    render_height = max(1, int(round(element_camera.data.ortho_scale * pixels_per_unit_x * RETINA_SCALE)))
    # Camera Data properties take precedence over Object properties.
    width_override = obj.data.get("Camera Resolution X", obj.get("Camera Resolution X"))
    # Camera Resolution X is relative to BASE_WIDTH: 800 keeps the square crop, smaller values narrow it.
    width_ratio = float(width_override) / BASE_WIDTH if width_override is not None else 1.0
    if width_ratio <= 0.0:
        raise ValueError(f"{cam_name}: Camera Resolution X must be greater than 0.")
    render_width = max(1, int(round(render_height * width_ratio)))
    print(f"{cam_name}: Camera Resolution X = {width_override}, rendering at {render_width} x {render_height}")

    # With vertical sensor fit, ortho_scale spans the height, so reducing the width crops horizontally.
    elem_height_1x = element_camera.data.ortho_scale * pixels_per_unit_y
    elem_width_1x = element_camera.data.ortho_scale * (render_width / render_height) * pixels_per_unit_x

    juce_center_x = center.x * BASE_WIDTH
    juce_center_y = (1.0 - center.y) * BASE_HEIGHT
    
    juce_x = int(round(juce_center_x - (elem_width_1x / 2.0)))
    juce_y = int(round(juce_center_y - (elem_height_1x / 2.0)))
    juce_w = int(round(elem_width_1x))
    juce_h = int(round(elem_height_1x))
    
    layout_data[cam_name] = {
        "setBounds": f"{juce_x}, {juce_y}, {juce_w}, {juce_h}",
        "x": juce_x, "y": juce_y, "width": juce_w, "height": juce_h,
        "frames": frames
    }

    # 2. RENDER FRAMES
    scene.camera = obj
    scene.render.resolution_x = render_width
    scene.render.resolution_y = render_height
    
    rendered_images = []
    original_sensor_fit = obj.data.sensor_fit
    try:
        obj.data.sensor_fit = 'VERTICAL'
        for f in range(startFrame, startFrame + frames):
            scene.frame_set(f)
            filepath = os.path.join(tmp_dir, f"{cam_name}_{f:04d}.png")
            scene.render.filepath = filepath
            bpy.ops.render.render(write_still=True)
            rendered_images.append(filepath)
    finally:
        obj.data.sensor_fit = original_sensor_fit

    # 3. STITCH FILMSTRIP (Using NumPy for speed)
    sheet_width = render_width
    sheet_height = render_height * frames
    sheet_pixels = np.zeros(sheet_width * sheet_height * 4, dtype=np.float32)

    for i, filepath in enumerate(rendered_images):
        img = bpy.data.images.load(filepath)
        frame_pixels = np.zeros(render_width * render_height * 4, dtype=np.float32)
        img.pixels.foreach_get(frame_pixels)
        frame_pixels = frame_pixels.reshape((render_height, render_width, 4))
        
        y_start = sheet_height - (i + 1) * render_height
        y_end = sheet_height - i * render_height
        
        sheet_3d = sheet_pixels.reshape((sheet_height, sheet_width, 4))
        sheet_3d[y_start:y_end, :, :] = frame_pixels
        bpy.data.images.remove(img)

    # 4. SAVE FILMSTRIP
    if cam_name in bpy.data.images:
        bpy.data.images.remove(bpy.data.images[cam_name])
        
    sheet_img = bpy.data.images.new(name=cam_name, width=sheet_width, height=sheet_height, alpha=True)
    sheet_img.pixels.foreach_set(sheet_pixels)
    
    final_path = os.path.join(OUTPUT_DIR, f"{cam_name}_filmstrip.png")
    sheet_img.filepath_raw = final_path
    sheet_img.file_format = 'PNG'
    sheet_img.save()
    print(f"Saved {cam_name}_filmstrip.png")

# 5. EXPORT LAYOUT JSON
json_path = os.path.join(OUTPUT_DIR, "juce_layout.json")
with open(json_path, 'w') as f:
    json.dump(layout_data, f, indent=4)

print(f"\nBatch complete! Coordinates saved to: {json_path}")
