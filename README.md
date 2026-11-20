# Shader Warmup

**Works with Unreal Engine 5.8.**

A boot screen for Unreal Engine 5. When your game starts, it loads every material and effect in your project, waits
until their shaders are compiled, and then opens your first level. Players get a progress bar instead of hitches the
first time something new appears on screen.

You can restyle the screen in the UMG Designer without writing any code.

### How the game starts

1. The game opens **`L_ShaderWarmup`** first (your project's **Game Default Map**).
2. When the shaders are ready, it opens your **Next Level** (Project Settings > Plugins > Shader Warmup), usually
   the level that was your Game Default Map before: your main menu or first level.

Unreal's own **Transition Map** setting (Maps & Modes) isn't used for this; leave it as it is.

---

## 1. Install

**Easiest: VFS Unreal Project Build Tool > Import Plugin > Shader Warmup.** It copies the plugin into your project,
builds it, and does steps 4 and 5 below for you: your current Game Default Map becomes the **Next Level**, then
Game Default Map is set to `L_ShaderWarmup`. (If your project has no Game Default Map of its own, it changes
neither and lists what to set.)

To install it by hand instead: you need a **C++ project** (if yours is Blueprint-only, add any C++ class once: *Tools > New C++ Class*) and
Visual Studio with the "Game development with C++" workload.

1. Close the editor.
2. Copy the `ShaderWarmup` folder into your project's `Plugins` folder (make the folder if it doesn't exist):
   ```
   MyProject/
     MyProject.uproject
     Plugins/
       ShaderWarmup/
         ShaderWarmup.uplugin
   ```
3. Right-click `MyProject.uproject` > **Generate Visual Studio project files**, then open the project. Click **Yes**
   when it asks to rebuild the missing modules.
4. **Edit > Project Settings > Maps & Modes**: set **Game Default Map** to `L_ShaderWarmup`.
   (In the Content Browser, turn on *Settings > Show Plugin Content* to see it.)
5. **Edit > Project Settings > Plugins > Shader Warmup**: set **Next Level** to your main menu or first level.

Done. Launch the game (Standalone, or a packaged build) and the warm-up screen comes first.

> Don't set `L_ShaderWarmup` as your **Editor Startup Map**. You don't need the screen while you work in the editor.

---

## 2. Change how it looks

Open `WBP_ShaderWarmup_Default` (in the plugin's content), or better: **duplicate it into your own Content folder**
first, then pick your copy in *Project Settings > Plugins > Shader Warmup > Widget Class*. Plugin updates will then
never overwrite your screen.

### The Details panel (no Designer work)

In the Widget Blueprint, click the **top entry in the Hierarchy** (the widget's own name). Under **Shader Warmup** in
the Details panel:

| Setting | What it does |
|---|---|
| Title Image | A texture or material for `TitleImage` (e.g. your game's logo). Empty = no title in the game |
| Title Image Height | Height of `TitleImage`; the width follows the picture's shape (0 = the Designer's size) |
| Background Image | A texture or material for `Background` |
| Bar Color | Fill colour of `ProgressBar` (tick the box to use it) |
| Bar Smoothing | How smoothly the bar slides (0 = jumps) |
| Preview Progress | How full the bar looks in the Designer, so you can style it |
| Text > ... | Every message. `{Percent}`, `{Left}` (shaders left) and `{Assets}` are filled in for you |
| Tips | Lines that rotate in `TipText` every *Seconds Per Tip* |

### The Designer

Move, resize, recolour, change fonts, add images or anything else. The plugin finds these widgets **by name** and
keeps them updated:

| Name | Type | Shows |
|---|---|---|
| `ProgressBar` | Progress Bar | 0 → 100% |
| `PercentText` | Text | "64%" |
| `StatusText` | Text | "Compiling shaders… 1,204 left" |
| `TitleImage` | Image | your Title Image (a white box in the Designer until you pick one) |
| `TipText` | Text | the Tips |
| `Background` | Image | your Background Image |

All of them are optional. Delete one and nothing breaks; rename one and it simply stops updating.

### Your own Widget Blueprint

*Add > User Interface > Widget Blueprint*, then pick **Shader Warmup Widget** as the parent class. Use the names
above, and/or implement these events in the Graph:

- **On Warmup Progress** (Progress 0..1, Shaders Remaining, Phase): every frame. Drive animations, materials, spinners.
- **On Warmup Finished**: once, just before the next level opens. Good for a fade-out.

Then select it in *Project Settings > Shader Warmup > Widget Class*.

---

## 3. Settings

*Project Settings > Plugins > Shader Warmup*

| Setting | Default | Meaning |
|---|---|---|
| Next Level | (none) | Level to open when done. **Set this.** |
| Widget Class | WBP_ShaderWarmup_Default | The screen |
| Folders To Warm Up | /Game | Folders whose materials / effects are loaded. Narrow it if your project has huge unused packs. |
| Include Particle Effects | on | Also load Niagara and Cascade systems |
| Min Seconds | 1 | Screen stays at least this long |
| Max Seconds | 300 | Gives up and moves on after this long |
| Quiet Seconds | 0.5 | Nothing may be left for this long before it counts as done |
| Finished Hold Seconds | 0.25 | Full bar shows this long before the level changes |
| Loading Share | 0.25 | Part of the bar used for loading (the rest is compiling) |
| Fast Pipeline Compile | on | Compile PSOs at full speed while the screen is up |
| Skip In Play In Editor | off | Skip straight to Next Level when you press Play on the warm-up map |
| Verbose Log | off | Progress lines in the log (`LogShaderWarmup`) |

Command line: `-SkipShaderWarmup` skips the wait.

### Blueprint nodes

Get them from **Get Game Instance Subsystem > Shader Warmup Subsystem**:
`Start Warmup`, `Cancel Warmup`, `Get Progress`, `Get Phase`, `Get Shaders Remaining`, `Is Running`, plus the
**On Progress** / **On Finished** events. Use these to run the warm-up in your own boot level instead of
`L_ShaderWarmup`, or set `ShaderWarmupGameMode` as that level's *GameMode Override*.

---

## 4. What it can and can't do

- **Editor and Standalone:** materials compile their shaders while they load. The screen shows them loading;
  on a cold cache that can take a while the first time.
- **Packaged game:** shaders are compiled when you package, so what's left is **PSO** (pipeline state) work for the
  graphics card. It's slow only on a player's **first launch** (and after a driver update). Later launches take
  about a second.
- **Global shaders** (the engine's own) compile before the first frame, so a black window right after an engine
  update is normal. No plugin can show a screen there.
- Only assets in the chosen folders are warmed up. Materials created at runtime (dynamic material instances of those
  materials are fine) or anything in folders you left out can still hitch.

### Extra: record a PSO cache (optional, advanced)

For the smoothest first launch, record the pipeline states your game really uses:

1. Project Settings > Packaging: turn on **Share Material Shader Code**. In `Config/DefaultEngine.ini` add:
   ```
   [DevOptions.Shaders]
   NeedsShaderStableKeys=true
   ```
2. Package, then play the packaged game with `-logPSO` on the command line and visit every level / effect.
3. Run `ShaderPipelineCacheTools expand` on the recorded `.rec.upipelinecache` files and the cook's `.shk` files
   to make a `.spc` file. Put it in `Build/Windows/PipelineCaches/` and package again.

The warm-up screen counts and compiles that cache automatically.

---

## 5. Troubleshooting

| Problem | Fix |
|---|---|
| Screen shows, then nothing happens | *Next Level* isn't set. The log says `no Next Level set`. |
| Black screen, no widget | *Widget Class* is empty or points at a deleted asset. |
| Bar sits at 0% for a few seconds in the editor | The editor is still scanning content. It continues by itself. |
| Plugin content not visible | Content Browser > Settings > *Show Plugin Content*. |
| "Modules are missing or built with a different engine version" | Click Yes to rebuild (needs Visual Studio). |
| The game goes straight to Next Level | You started with `-SkipShaderWarmup`, or *Skip In Play In Editor* is on. |
