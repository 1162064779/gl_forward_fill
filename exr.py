import OpenEXR
import Imath
import numpy as np
from PIL import Image  # 用于保存 PNG

def load_exr(filename):
    exr_file = OpenEXR.InputFile(filename)
    dw = exr_file.header()['dataWindow']
    width = dw.max.x - dw.min.x + 1
    height = dw.max.y - dw.min.y + 1

    # 读取 'Z'（深度）通道
    pt = Imath.PixelType(Imath.PixelType.FLOAT)
    raw = exr_file.channel('Z', pt)
    depth = np.frombuffer(raw, dtype=np.float32).reshape((height, width))
    return depth

def save_gray_png(depth: np.ndarray, output_path: str):
    # 计算 min/max
    d_min = np.min(depth)
    d_max = np.max(depth)
    print(f"[INFO] Depth range: min={d_min:.4f}, max={d_max:.4f}")

    # 避免除以0
    if d_max - d_min < 1e-6:
        d_max = d_min + 1e-6

    # 归一化到 0~255
    depth_norm = (depth - d_min) / (d_max - d_min)
    depth_gray = (depth_norm * 255.0).astype(np.uint8)

    # 保存为灰度 PNG
    img = Image.fromarray(depth_gray, mode='L')
    img.save(output_path)
    print(f"[INFO] Saved grayscale depth to: {output_path}")

# 主流程
depth = load_exr("depth.exr")
print(f"[INFO] depth.shape = {depth.shape}")
print(f"[INFO] Depth min: {depth.min()}, max: {depth.max()}")

np.set_printoptions(precision=4, suppress=True, linewidth=200)
print("[INFO] Depth values:")
print(depth)

# 保存为灰度图
save_gray_png(depth, "exr_depth.png")