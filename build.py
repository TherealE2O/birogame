#!/usr/bin/env python3
import os
import sys
import glob
import time
import subprocess

def main():
    root = os.path.dirname(os.path.abspath(__file__))
    if os.path.exists(os.path.join(root, "engine", "src")):
        engine_src = os.path.join(root, "engine", "src")
        engine_inc = os.path.join(root, "engine", "include")
    else:
        engine_src = os.path.join(root, "..", "engine", "src")
        engine_inc = os.path.join(root, "..", "engine", "include")
        
    if os.path.exists(os.path.join(root, "extern", "raylib")):
        raylib_inc = os.path.join(root, "extern", "raylib", "include")
        raylib_lib = os.path.join(root, "extern", "raylib", "lib", "libraylib.a")
    else:
        raylib_inc = os.path.join(root, "..", "extern", "raylib", "include")
        raylib_lib = os.path.join(root, "..", "extern", "raylib", "lib", "libraylib.a")
    
    src_main = os.path.join(root, "src", "main.c")
    out_exe = os.path.join(root, "biro_game.exe")
    
    b2_sources = glob.glob(os.path.join(engine_src, "*.c"))
    if not b2_sources:
        print("Error: Could not find Box2D v3 sources in", engine_src)
        sys.exit(1)
        
    print(f"Building Biro Game (Box2D v3 + Raylib 6.0)...")
    print(f"  Box2D v3 sources: {len(b2_sources)} files")
    print(f"  Main source:      {src_main}")
    print(f"  Output binary:    {out_exe}")
    
    cmd = [
        "python", "-m", "ziglang", "cc",
        "-O2",
        "-Wall",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-missing-braces",
        src_main,
        *b2_sources,
        f"-I{engine_inc}",
        f"-I{engine_src}",
        f"-I{raylib_inc}",
        raylib_lib,
        "-lopengl32",
        "-lgdi32",
        "-lwinmm",
        "-o", out_exe
    ]
    
    t0 = time.time()
    res = subprocess.run(cmd)
    t1 = time.time()
    
    if res.returncode == 0:
        exe_size = os.path.getsize(out_exe) / (1024 * 1024)
        print(f"\n[SUCCESS] Build completed in {t1 - t0:.2f}s! Executable size: {exe_size:.2f} MB")
        
        if "--run" in sys.argv:
            print(f"Launching {out_exe}...")
            subprocess.run([out_exe])
    else:
        print(f"\n[FAILED] Compilation failed with return code {res.returncode}")
        sys.exit(res.returncode)

if __name__ == "__main__":
    main()
