#!/usr/bin/env python3
"""
=============================================================================
BIRO CLASH: School Desk Physics - WebAssembly Compiler Script
Compiles Box2D v3.2.0 + Raylib 6.0 WebAssembly to static web assets
=============================================================================
"""

import os
import sys
import glob
import shutil
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
    
    if os.path.exists(os.path.join(root, "extern", "raylib_web", "raylib-6.0_webassembly")):
        raylib_web_dir = os.path.join(root, "extern", "raylib_web", "raylib-6.0_webassembly")
    else:
        raylib_web_dir = os.path.join(root, "..", "extern", "raylib_web", "raylib-6.0_webassembly")
    raylib_web_inc = os.path.join(raylib_web_dir, "include")
    raylib_web_lib = os.path.join(raylib_web_dir, "lib", "libraylib.web.a")
    
    src_main = os.path.join(root, "src", "main.c")
    web_src_dir = os.path.join(root, "web")
    
    out_dir = os.path.join(root, "web_dist")
    if "--out" in sys.argv:
        idx = sys.argv.index("--out")
        if idx + 1 < len(sys.argv):
            out_dir = os.path.abspath(sys.argv[idx + 1])
            
    os.makedirs(out_dir, exist_ok=True)
    
    print("==================================================================")
    print("  BIRO CLASH: School Desk Physics - WebAssembly Build")
    print("  Box2D v3.2.0 + Raylib 6.0 WebAssembly (100% Free Static Web)")
    print("==================================================================")

    # 1. Verify sources & raylib
    b2_sources = glob.glob(os.path.join(engine_src, "*.c"))
    if not b2_sources:
        print(f"[ERROR] Could not find Box2D v3 sources in: {engine_src}")
        sys.exit(1)
        
    if not os.path.exists(raylib_web_lib):
        print(f"[ERROR] Could not find WebAssembly Raylib static library in: {raylib_web_lib}")
        sys.exit(1)
        
    print(f"  Box2D v3 sources:     {len(b2_sources)} C source files")
    print(f"  Game source:          {src_main}")
    print(f"  Raylib Web lib:       {raylib_web_lib}")
    print(f"  Output directory:     {out_dir}")

    # 2. Check for emcc compiler
    emcc_bin = shutil.which("emcc")
    if not emcc_bin:
        print("\n[NOTE] 'emcc' (Emscripten SDK) is not detected in your local PATH.")
        print("  - To build locally: Install Emscripten via 'emsdk' (https://emscripten.org).")
        print("  - For GitHub Pages: The automated workflow (.github/workflows/deploy.yml)")
        print("    runs 'emcc' on GitHub Actions runners and publishes directly to Pages for free!\n")
        
        # Copy web static assets anyway so the folder structure is ready
        copy_web_assets(web_src_dir, out_dir, root)
        return

    # 3. Formulate emcc command
    exported_funcs = [
        "_main",
        "_ApplyRemoteStrike",
        "_ApplyRemoteSync",
        "_SetMatchMode",
        "_SetAIDifficulty",
        "_SetOnlineRole",
        "_SetOnlineRoomCode",
        "_SetOnlineConnectionStatus",
        "_RestartMatchFromNetwork",
        "_GetGameActivePlayer",
        "_GetGameMatchState",
        "_malloc",
        "_free"
    ]
    
    exported_runtime = [
        "ccall",
        "cwrap",
        "stringToUTF8",
        "UTF8ToString",
        "allocateUTF8"
    ]
    
    out_js = os.path.join(out_dir, "biro_game.js")
    
    cmd = [
        emcc_bin,
        "-O2",
        "-DPLATFORM_WEB",
        "-DBOX2D_DISABLE_SIMD",
        "-Wall",
        "-Wno-unused-variable",
        "-Wno-unused-function",
        "-Wno-missing-braces",
        src_main,
        *b2_sources,
        raylib_web_lib,
        f"-I{engine_inc}",
        f"-I{engine_src}",
        f"-I{raylib_web_inc}",
        "-s", "USE_GLFW=3",
        "-s", "ALLOW_MEMORY_GROWTH=1",
        "-s", "STACK_SIZE=1048576",
        "-s", f"EXPORTED_FUNCTIONS={exported_funcs}",
        "-s", f"EXPORTED_RUNTIME_METHODS={exported_runtime}",
        "-o", out_js
    ]

    print("\nCompiling with Emscripten...")
    t0 = time.time()
    res = subprocess.run(cmd)
    t1 = time.time()

    if res.returncode == 0:
        wasm_file = os.path.join(out_dir, "biro_game.wasm")
        wasm_size = os.path.getsize(wasm_file) / (1024 * 1024) if os.path.exists(wasm_file) else 0
        js_size = os.path.getsize(out_js) / 1024 if os.path.exists(out_js) else 0
        print(f"\n[SUCCESS] Emscripten compilation finished in {t1 - t0:.2f}s!")
        print(f"  WASM Binary: {wasm_size:.2f} MB")
        print(f"  JS Loader:   {js_size:.1f} KB")
        
        copy_web_assets(web_src_dir, out_dir, root)
        print(f"\n[READY] Game ready in '{out_dir}'.")
        print("  To test locally, run: python -m http.server --directory web_dist 8000")
    else:
        print(f"\n[FAILED] Emscripten compilation failed with code {res.returncode}")
        sys.exit(res.returncode)

def copy_web_assets(web_src_dir, out_dir, root):
    for f in ["index.html", "style.css", "multiplayer.js"]:
        src_path = os.path.join(web_src_dir, f)
        if os.path.exists(src_path):
            shutil.copy2(src_path, os.path.join(out_dir, f))
            print(f"  Copied {f} -> {out_dir}")
            
    # Copy preview image for rich social card embeds (Twitter/OpenGraph)
    preview_img = os.path.join(root, "test_ai_gameplay.png")
    if os.path.exists(preview_img):
        shutil.copy2(preview_img, os.path.join(out_dir, "preview.png"))
        print(f"  Copied social preview card image -> {out_dir}/preview.png")

if __name__ == "__main__":
    main()
