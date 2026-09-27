# Biro Clash

A 2D physics game based on the pen-flicking game we played on school desks growing up.

Built in C with **Box2D v3** and **Raylib**, compiled to WebAssembly for the browser. Multiplayer runs peer-to-peer over WebRTC via PeerJS, so it runs completely serverless.

▶ **Play online**: [thereale2o.github.io/birogame](https://thereale2o.github.io/birogame/)

---

## How to Play

Take turns flicking your biro across the desk. The goal is simple: knock the other player's pen off the table without flying off yourself.

- **Desktop (Mouse)**: Hover over your pen to pick a contact point. Hold left-click to set the angle and charge power, then release to flick.
- **Mobile (Touch)**: Swipe through the pen. The speed of your swipe determines the power, and where your swipe cuts through the pen dictates the angle and spin.
- **Game Modes**:
  - Solo vs AI bot
  - Local pass & play (same keyboard/mouse)
  - Online peer-to-peer (share the room link with a friend)

---

## Running Locally

### Desktop (Windows)
Requires `zig` or any C compiler in your PATH:
```bash
python build.py --run
```

### Web (WebAssembly)
Requires Emscripten (`emsdk`):
```bash
python build_web.py
python -m http.server --directory web_dist 8000
```
Then open `http://localhost:8000` in your browser.

---

## Tech Stack
- **Engine**: C99
- **Physics**: [Box2D v3](https://github.com/erincatto/box2d)
- **Graphics/Audio**: [Raylib 6.0](https://www.raylib.com/)
- **Web Build**: Emscripten (WASM)
- **Networking**: PeerJS (WebRTC DataChannels)
