# ZuCompleluxe UI Layout Configuration Specification (`properties/layout.xml`)

## Overview

In the original *Zuma Deluxe* (v1.0.0.1) binary released in 2003, all user interface coordinates, button placements, temple structures, and waypoint targets were hardcoded as absolute immediate values embedded directly within compiled x86 machine instructions.

**ZuCompleluxe** completely decouples the entire presentation layer from the binary through `modules/zumareshandler.dll` and `modules/zumatestingkit.dll`. All interface elements across all 17 distinct game screens, dialogs, and DevTester sidecar features are exposed via an intuitive, human-readable configuration file: `properties/layout.xml`.

This allows total aesthetic and layout control:
* Custom high-definition (HD) UI overhauls.
* Repositioned navigation buttons and temple doorways.
* Configurable Secret Temple 5 ("The Void", Stage 13) placement and door waypoint navigation.
* Tailored In-Game Menubar stone HUD elements and safety margins across arbitrary widescreen and ultrawide aspect ratios (16:9, 16:10, 21:9 ultrawide, 32:9 super ultrawide, and vertical 9:16 displays).
* Complete immunity to hardcoded coordinate drift across arbitrary display resolutions (such as 1200x675, 1920x1080, 2560x1440, and 3840x2160 4K).

---

## The Virtual 640x480 Canonical Coordinate System

All coordinates declared in `properties/layout.xml` are authored in PopCap's standard **640x480 virtual canvas space**.

### Dynamic Transformation Pipeline
At runtime, `zumareshandler.dll` reads the XML file upon initialization and transforms every coordinate into target display coordinates using sub-pixel scaling:

$$\text{scale} = \min\left(\frac{W_{\text{target}}}{640}, \frac{H_{\text{target}}}{480}\right)$$

$$X_{\text{screen}} = \lfloor x_{\text{virtual}} \times \text{scale} + 0.5 \rfloor$$

$$Y_{\text{screen}} = \lfloor y_{\text{virtual}} \times \text{scale} + 0.5 \rfloor$$

### Why Use 640x480 Coordinates?
1. **Resolution Independence**: You author your layouts once in 640x480. The engine automatically renders your layout identically whether the player runs the game at 800x600, 1200x675, 1920x1080, or 4K.
2. **Vanilla Asset Compatibility**: Pixel coordinates align 1:1 with vanilla sprites and art assets extracted from the game's PAK archives.
3. **Aspect-Correct Framing**: Screen-edge elements (such as HUD banners, In-Game Menubar widgets, and the Credits "Whatever" button) dynamically anchor to actual viewport borders while maintaining proportions.

---

## Universal Adaptive Resolution Scaling Engine

ZuCompleluxe features a mathematically sound, resolution-independent layout engine. You author layouts once using standard PopCap canonical **640x480 coordinates**, and the engine adaptively positions every element across **ANY aspect ratio**:
* **4:3 Standard Displays** (e.g. 640x480, 800x600, 1024x768, 1600x1200)
* **3:2 Displays** (e.g. 1152x768, 1920x1280, 2160x1440, Microsoft Surface)
* **16:9 Widescreen Displays** (e.g. 1200x675, 1280x720, 1920x1080, 2560x1440, 3840x2160 4K)
* **16:10 Displays** (e.g. 1280x800, 1680x1050, 1920x1200, Steam Deck)
* **21:9 Ultrawide Displays** (e.g. 2560x1080, 3440x1440, 5120x2160)

### How Aspect Adaptation Works:
1. **Aspect Ratio Preservation**:
   $$\text{scale} = \min\left(\frac{W_{\text{target}}}{640}, \frac{H_{\text{target}}}{480}\right)$$
   $$cw = \lfloor 640 \times \text{scale} + 0.5 \rfloor, \quad ch = \lfloor 480 \times \text{scale} + 0.5 \rfloor$$
   $$xo = \frac{W_{\text{target}} - cw}{2}, \quad yo = \frac{H_{\text{target}} - ch}{2}$$
2. **Main Menu, Map Screens & Menus**:
   * Screens are centered with offset $(xo, yo)$.
   * Buttons, doorways, and text are placed at local offsets `PARENT_LOCAL_X(x) = x * scale` and `PARENT_LOCAL_Y(y) = y * scale`.
   * This guarantees that menus and buttons never warp, distort, or drift out of position regardless of whether the aspect ratio is 4:3, 3:2, 16:9, or 21:9.
3. **Universal Dialog Windows (Continue, Options, User Selection, etc.)**:
   * When on menus (`board == NULL`), dialogs are geometrically centered relative to the 4:3 canvas via $x = (cw - w) / 2$ and $y = (ch - h) / 2$. PopCap's widget parent then positions the modal perfectly in the optical center of the screen, never colliding with or overlapping side buttons.
   * When in-game (`board != NULL`), dialogs center across the full display $(W_{\text{target}} - w) / 2$ and $(H_{\text{target}} - h) / 2$.
   * Clamped by `minTop` from `layout.xml` to prevent collision with top headers.

---

## Adventure Doors Disambiguation: `doorX, doorY` vs `frogX, frogY`

### The Problem with Legacy `x1, y1, x2, y2`
In vanilla *Zuma Deluxe*, each door on the Adventure Map was tracked internally inside PopCap's `AdventureDoor` struct:

```cpp
struct AdventureDoor {
    int mX1; // Offset +0x00
    int mY1; // Offset +0x04
    int mX2; // Offset +0x08
    int mY2; // Offset +0x0C
};
```

When early reverse-engineering tools extracted this structure, they dumped the members as `<Door id="0" x1="226" y1="300" x2="256" y2="361" />`.

This caused widespread confusion among modders:
* **Misconception**: Modders assumed `(x1, y1)` and `(x2, y2)` formed a rectangular bounding box (e.g. top-left to bottom-right, or position + dimensions).
* **Reality**: It is **NOT** a rectangle or bounding box.

### The True Functional Roles

```
        +-------------------------+
        |   IMAGE_DOOR_...        |
        |   (doorX, doorY)        |
        |      [===DOOR===]       |
        +-------------------------+
                     |
                     |  (Path distance)
                     v
               [GOLDEN FROG]
               (frogX, frogY)
```

1. **`doorX`, `doorY` (Door Graphic Position)**:
   * The exact pixel coordinate on the temple map where the entrance doorway sprite (`IMAGE_DOOR_...` or `IMAGE_ADVDOOR5`) is rendered.
   * Modifying these coordinates physically moves the stone doorway arch graphic.
2. **`frogX`, `frogY` (Player Frog Waypoint)**:
   * The waypoint coordinate on the ground path where the golden frog pawn stands/sits when this door is selected.
   * When moving between levels, the frog hops to `(frogX, frogY)`.
   * Modifying these coordinates adjusts where the frog sits without altering the door graphic.

### Full Backwards Compatibility
`zumareshandler.dll` supports **both** naming conventions. Modders are strongly advised to use the new explicit attribute names:

```xml
<!-- RECOMMENDED (Intuitive & Explicit) -->
<Door id="0" doorX="226" doorY="300" frogX="256" frogY="361" />

<!-- SUPPORTED (Legacy Backward Compatibility) -->
<Door id="0" x1="226" y1="300" x2="256" y2="361" />
```

### Adventure Map Doors Reference Table

| Door ID | Temple | Stage Numbers | `doorX` (`x1`) | `doorY` (`y1`) | `frogX` (`x2`) | `frogY` (`y2`) | Notes |
|:---:|:---|:---:|:---:|:---:|:---:|:---:|:---|
| `0` | Temple 1 (Quetzalcoatl) | Stage 1 | `226` | `300` | `256` | `361` | Starting Aztec portal |
| `1` | Temple 1 (Quetzalcoatl) | Stage 2 | `296` | `270` | `327` | `364` | Mid-tier stair portal |
| `2` | Temple 1 (Quetzalcoatl) | Stage 3 | `366` | `300` | `398` | `360` | Upper plaza portal |
| `3` | Temple 2 (Popocatepetl) | Stage 4 | `0` | `164` | `56` | `218` | Left magma portal |
| `4` | Temple 2 (Popocatepetl) | Stage 5 | `69` | `159` | `116` | `201` | Mid magma portal |
| `5` | Temple 2 (Popocatepetl) | Stage 6 | `120` | `155` | `167` | `188` | Inner caldera portal |
| `6` | Temple 3 (Centeotl) | Stage 7 | `452` | `154` | `462` | `188` | Low marsh portal |
| `7` | Temple 3 (Centeotl) | Stage 8 | `509` | `154` | `521` | `192` | Mid swamp portal |
| `8` | Temple 3 (Centeotl) | Stage 9 | `568` | `154` | `584` | `196` | High altar portal |
| `9` | Temple 4 (Sun God) | Stage 10 | `220` | `35` | `248` | `86` | Left cosmic doorway |
| `10` | Temple 4 (Sun God) | Stage 11 | `291` | `35` | `324` | `86` | Center celestial doorway |
| `11` | Temple 4 (Sun God) | Stage 12 | `369` | `35` | `408` | `85` | Right apex doorway |
| `12` | Temple 5 (The Void) | Stage 13 | `291` | `176` | `323` | `201` | Secret Temple 5 portal |

---

## Complete XML Configuration Directory

Below is the complete reference of all 16 configuration blocks available in `properties/layout.xml`.

### 1. `<AdventureTemples>`
Controls the rendering positions of background temple structures on the Adventure Map.

```xml
<AdventureTemples>
    <Temple1 x="0" y="188" />
    <Temple2 x="0" y="112" />
    <Temple2V x="0" y="112" />
    <Temple3 x="395" y="125" />
    <Temple3V x="395" y="125" />
    <Temple4 x="174" y="1" />
    <Temple5 x="126" y="148" />
    <Middle x="0" y="72" />
</AdventureTemples>
```
* `Temple1` through `Temple4`: Active temple stone facades.
* `Temple2V`, `Temple3V`: Vegetation / overgrown variants drawn before unlocking subsequent temples.
* `Temple5`: Secret Temple 5 (`IMAGE_ADVTEMPLE5`, 391x100) leading to Stage 13 ("The Void").
* `Middle`: Central decorative plateau plate (`IMAGE_ADVMIDDLE`).

---

### 2. `<AdventureDoors>`
Defines the 13 temple doors (0..12) and their corresponding frog waypoint stations.

```xml
<AdventureDoors>
    <Door id="0" doorX="226" doorY="300" frogX="256" frogY="361" />
    ...
    <Door id="12" doorX="291" doorY="176" frogX="323" frogY="201" />
</AdventureDoors>
```
* `Door id="0"` through `id="11"`: Vanilla stages 1..12.
* `Door id="12"`: Secret Temple 5 Door (`IMAGE_ADVDOOR5`, 64x50) for Stage 13.

---

### 3. `<AdventureButtons>`
Defines navigation pushbuttons on the Adventure Map.

```xml
<AdventureButtons>
    <MainMenu x="0" y="438" />
    <Play x="542" y="441" />
</AdventureButtons>
```
* `MainMenu`: "MAIN MENU" stone button (bottom-left).
* `Play`: "PLAY" stone button (bottom-right).

---

### 4. `<AdventureLabels>`
Controls text labels, level titles, high score tables, stage info box, and the Stage 13 Banner.

```xml
<AdventureLabels>
    <HighscoreTitle x="456" />
    <BottomBox x="289" y="382" />
    <StageNum x="344" y="396" />
    <StageText y="422" centerX="326" />
    <Subtitle y="456" centerX="326" />
    <HighscoreScores x="575" />
    <HighscoreDate x="567" />
    <HighscoreNames x="470" />
    <InfoBox x="20" y="60" w="150" h="100" />
    <Stage13Badge x="289" y="382" />
    <Stage13Num x="344" y="396" />
    <Stage13Title x="326" y="422" />
    <Stage13Subtitle x="326" y="456" />
</AdventureLabels>
```
* `HighscoreTitle`: Title header X coordinate for the high score ledger.
* `BottomBox`: Stone backdrop plate for the current stage name (`IMAGE_ADVSTAGE`).
* `StageNum`: Stage number digit display position.
* `StageText`, `Subtitle`: Centered text labels (`centerX` defines horizontal anchor point).
* `HighscoreScores`, `HighscoreDate`, `HighscoreNames`: Tabular columns for the top score ledger.
* `InfoBox`: Semi-transparent briefing panel (`x, y, w, h`).
* `Stage13Badge`: Stone banner plate when Temple 5 is selected (`IMAGE_ADVSTAGE`).
* `Stage13Num`: Number "13" text anchor.
* `Stage13Title`: "THE VOID" headline anchor.
* `Stage13Subtitle`: "THE PLACE OF NO ENDING" subtitle anchor.

---

### 5. `<GauntletDoors>`
Defines the 4 practice temple door portals in Gauntlet Mode.

```xml
<GauntletDoors>
    <Door id="0" x="209" y="179" />
    <Door id="1" x="232" y="129" />
    <Door id="2" x="249" y="82" />
    <Door id="3" x="265" y="28" />
</GauntletDoors>
```
* `Door id="0"`: Quetzalcoatl portal.
* `Door id="1"`: Popocatepetl portal.
* `Door id="2"`: Centeotl portal.
* `Door id="3"`: Sun God portal.

---

### 6. `<GauntletButtons>`
Defines navigation and difficulty selection buttons in Gauntlet Mode.

```xml
<GauntletButtons>
    <Practice x="177" y="444" />
    <Survival x="277" y="444" />
    <Back x="390" y="444" />
    <MainMenu x="10" y="422" />
    <Next x="506" y="413" />
    <Play x="505" y="446" />
</GauntletButtons>
```
* `Practice` & `Survival`: Mode selection tabs.
* `Back` & `Next`: Stage selection carousel controls.
* `MainMenu`: Return to title screen.
* `Play`: Launch selected gauntlet course.

---

### 7. `<GauntletLabels>`
Controls difficulty badges, door preview frames, custom map tags, and high score displays.

```xml
<GauntletLabels>
    <DoorFrame x="170" y="220" />
    <HighscoreTitle x="434" />
    <DoorName x="170" y="427" />
    <PracticeText x="150" y="260" w="320" />
    <CustomMapTag x="150" y="260" />
    <Gem x="560" y="220" />
    <HighscoreScores x="575" />
    <HighscoreNames x="445" />
    <HighscoreClip w="567" />
    <InfoBox x="20" y="60" w="150" h="100" />
</GauntletLabels>
```
* `DoorFrame`: Thumbnail preview border for the selected level.
* `CustomMapTag`: Upper-left corner for the custom map badge text/tag.
* `Gem`: Difficulty rank jewel badge (Rabbit, Eagle, Jaguar, Sun God).
* `HighscoreClip`: Maximum clipping boundary width for long player names.

---

### 8. `<MainMenu>`
Controls main title screen buttons, interactive frog facial animations, solar flares, and pupil raycasts.

```xml
<MainMenu>
    <AdventureBtn x="452" y="64" />
    <GauntletBtn x="436" y="153" />
    <OptionsBtn x="418" y="236" />
    <MoreGamesBtn x="394" y="305" />
    <QuitBtn x="496" y="315" />
    <FrogEyes x="191" y="331" />
    <EyeBlink x="188" y="247" />
    <Sun1 x="-63" y="-69" />
    <Sun2 x="-63" y="-69" />
    <EyeRay1 x="29" y="25" />
    <EyeRay2 x="87" y="16" />
    <EyeIdle1 x="219" y="368" />
    <EyeIdle2 x="276" y="355" />
</MainMenu>
```
* `AdventureBtn` through `QuitBtn`: The 5 primary menu options.
* `FrogEyes`: Base coordinates for dynamic mouse-tracking pupils.
* `EyeBlink`: Idle eyelid blinking animation origin.
* `Sun1`, `Sun2`: Solar flare animated rays emitted behind the golden frog statue.
* `EyeRay1`, `EyeRay2`: Starting vectors for line-of-sight raycasts from left and right eye sockets.
* `EyeIdle1`, `EyeIdle2`: Resting pupil positions when mouse cursor is off-screen.

---

### 9. `<MoreGames>`
Defines quadrant boundaries for the 4 promo game preview cards.

```xml
<MoreGames>
    <Thumb0 x="58" y="107" w="250" h="150" />
    <Thumb1 x="330" y="107" w="250" h="150" />
    <Thumb2 x="58" y="277" w="250" h="150" />
    <Thumb3 x="330" y="277" w="250" h="150" />
    <BackBtn x="164" y="445" />
</MoreGames>
```

---

### 10. `<LoadingScreen>`
Controls the initial loading screen progress elements.

```xml
<LoadingScreen>
    <LoadingBar x="128" y="349" />
    <ClickToPlay y="430" />
</LoadingScreen>
```
* `LoadingBar`: Top-left origin of the fillable stone progress bar.
* `ClickToPlay`: Vertical baseline for the pulsing "CLICK HERE TO PLAY!" button.

---

### 11. `<InGameHUD>`
Controls in-game banners, level announcements, stone menubar layout, and viewport margins.

```xml
<InGameHUD>
    <LevelBanner offsetX="40" offsetY="40" />
    <LivesBanner centerX="320" centerY="240" />
    <Menubar livesX="28" stageCX="200" scoreRX="360" zumaX="409" menuX="540" />
    <MenubarMargins top="50" bottom="100" left="200" right="150" />
</InGameHUD>
```
* `LevelBanner`: Distance in pixels from the bottom-right viewport edge for level announcements (`RABBIT 1`, `STAGE 4-1`).
* `LivesBanner`: Canonical centerpoint for large announcements (`%d LIVES LEFT`, `LAST LIFE!`, `GAME OVER`). Setting `320, 240` guarantees exact dead-center placement across all widescreen aspect ratios.
* `Menubar`: Horizontal positions for stone menubar widgets:
  * `livesX`: Leftmost frog lives icons start position.
  * `stageCX`: Center X coordinate for current stage indicator.
  * `scoreRX`: Right-aligned boundary for numerical score display.
  * `zumaX`: Zuma progression meter graphic X position.
  * `menuX`: In-game "MENU" button X position.
* `MenubarMargins`: Viewport margins (`top`, `bottom`, `left`, `right`) defining safe playfield boundaries.

---

### 12. `<StageComplete>`
Controls cutscene typography, particle animations, and golden frog hop waypoints.

```xml
<StageComplete>
    <StageText x="320" y="150" align="top-center" />
    <CompleteText x="320" y="250" align="top-center" />
    <FrogWriting y="250" align="top-center" />
    <FrogWaypoints bx="150" cx="500" dx="320" y="250" align="center" />
    <FrogTarget x="320" y="200" align="center" />
</StageComplete>
```
* `StageText`: Center X, vertical baseline Y, and alignment for the "STAGE %d" text.
* `CompleteText`: Center X, vertical baseline Y, and alignment for the sparkling "COMPLETE" banner and particles.
* `FrogWriting`: Baseline Y and alignment where the golden frog hops while drawing each character of "STAGE".
* `FrogWaypoints`: Waypoints and baseline for the frog's 3-step celebratory hop sequence:
  * `bx`: Hop 1 takeoff waypoint X coordinate.
  * `cx`: Hop 2 apex waypoint X coordinate.
  * `dx`: Final landing center waypoint X coordinate.
  * `y`: Vertical baseline Y coordinate for all waypoints.
  * `align`: Spatial alignment anchor (`center`, `top-center`, etc.).
* `FrogTarget`: Center X, landing altitude Y, and alignment for the golden frog during the final Stage 13-1 zoom-out animation.

---

### 13. `<TempleCleared>`
Controls all visual presentation elements, characters, dialogue boxes, and navigation controls on the celebratory Temple Cleared / Congratulations victory screen.

```xml
<TempleCleared>
    <ContinueBtn x="410" y="450" align="center" />
    <Title x="320" y="120" align="center" />
    <TikiHead x="50" y="160" align="center" />
    <DialogText x="220" y="180" width="380" height="150" align="center" />
    <FrogMedallion x="120" y="380" align="center" />
    <FrogReply x="200" y="280" align="center" />
</TempleCleared>
```

| Element | Attributes | Default | Description |
| :--- | :--- | :--- | :--- |
| `ContinueBtn` | `x`, `y`, `align` | `(410, 450, center)` | Coordinates and alignment for the modal green Aztec stone **"CONTINUE"** button. |
| `Title` | `x`, `y`, `align` | `(320, 120, center)` | Center X and baseline Y coordinate for the pulsating golden **"CONGRATULATIONS!"** banner. |
| `TikiHead` | `x`, `y`, `align` | `(50, 160, center)` | Center anchor coordinates for the floating, gently bobbing Tiki God avatar. |
| `DialogText` | `x`, `y`, `width`, `height`, `align` | `(220, 180, 380, 150, center)` | Position, bounding dimensions, and alignment for the Tiki God's multi-line congratulations speech text block. |
| `FrogMedallion` | `x`, `y`, `align` | `(120, 380, center)` | Center anchor coordinates for the player's stone Frog Medallion badge. |
| `FrogReply` | `x`, `y`, `align` | `(200, 280, center)` | Coordinates and alignment for the frog's dialogue response line (*"RIBBIT!"*). |

---

### 14. `<CreditsScreen>`
Controls the end-game scrolling credits crawl.

```xml
<CreditsScreen>
    <WhateverBtn offsetX="130" />
    <CreditsText centerX="320" />
    <ScrollStart y="480" />
</CreditsScreen>
```
* `WhateverBtn`: Distance in pixels from the right screen edge for the "WHATEVER" skip button.
* `CreditsText`: Horizontal centering axis for all scrolling attribution strings.
* `ScrollStart`: Vertical entry threshold where text begins its upward scroll.

---

### 15. `<HelpScreen>`
Controls the interactive instructions tablet, tutorial animations, and DevTester cheat controls.

```xml
<HelpScreen>
    <HelpBack x="11" y="51" />
    <Skull x="65" y="20" />
    <Coin x="282" y="45" />
    <Frog x="500" y="65" />
    <TopText1 x="30" y="40" />
    <TopText2 x="8" y="40" />
    <SwapClick x="560" y="55" />
    <SwapMouse x="580" y="60" />
    <BallTarget x="240" y="160" />
    <DevTemple5 x="225" y="114" />
    <DevMouseM x="465" y="145" />
</HelpScreen>
```
* `HelpBack`: Upper-left origin of `IMAGE_HELP_BACK` stone tablet.
* `Skull`: Danger demonstration compartment.
* `Coin`: Target practice coin icon and text.
* `Frog`: Ball swap tutorial pawn.
* `TopText1`, `TopText2`: Instruction header text positions uncovered from the stone frame.
* `SwapClick`: "CLICK!" text position demonstrating marble swapping.
* `SwapMouse`: Animated right-click mouse icon position.
* `BallTarget`: Destination point where demonstration marbles strike the chain.
* `DevTemple5`: Temple 5 preview graphic position on DevTester instructions Tab 3.
* `DevMouseM`: Middle mouse cheat button icon position on DevTester instructions Tab 3.

---

### 16. `<Dialogs>`
Controls universal centering clamps, alignments, and boundaries for in-game dialogs.

```xml
<Dialogs>
    <!-- Universal modal dialog centering and top margin clamp -->
    <Dialog minTop="10" align="center" />
    <!-- DevTester Custom Dialog settings -->
    <DevDialog width="380" minTop="10" align="center" />
</Dialogs>
```
* `Dialog`:
  * `minTop`: Minimum distance in pixels from the top screen edge when centering modal dialog boxes (Continue, Options, User Selection, Error messages), preventing dialogs from overlapping title banners or window titlebars.
  * `align`: Universal anchor mode (`center`, `top-middle`, `bottom-middle`). Defaults to `center` (ID 5).
* `DevDialog`:
  * `width`: Total pixel width for DevTester dialog window (ID 999). Defaults to 380.
  * `minTop`: Minimum top margin clamp for DevTester dialog. Defaults to 10.
  * `align`: Spatial alignment anchor for DevTester dialog.

---

### 17. `<DevHelpScreen>`
Full visual, textual, animation, and dialog specification for the **DevTester Instructions Tablet (Tab 3)** and Developer Testing Kit sidecar (`zumatestingkit.dll`).

```xml
<DevHelpScreen>
    <!-- Header title "DEVELOPER MODE" and subtitle -->
    <HeaderTitle x="320" y="28" align="top-middle" />
    <HeaderSubtitle x="320" y="78" align="top-middle" />

    <!-- Dual Column Base Centers (Left: Adventure/Cheats, Right: Gauntlet/Demo) -->
    <Columns col1X="162" col2X="478" />

    <!-- Section 1 (Left): Adventure Mode -->
    <AdvHeader y="108" align="top-middle" />
    <Temple5Preview x="67" y="114" align="top-left" />
    <AdvBullets x="30" y="206" spacing="18" align="top-left" />

    <!-- Section 2 (Right): Gauntlet Mode -->
    <GauntHeader y="108" align="top-middle" />
    <GauntBullets x="343" y="132" spacing="21" align="top-left" />

    <!-- Section 3 (Bottom Left): Middle Mouse Button Cheat -->
    <CheatHeader y="282" align="top-middle" />
    <CheatBullets x="30" y="304" spacing="18" align="top-left" />

    <!-- Section 4 (Bottom Right): Interactive 5-Ball Demo & Animation -->
    <DemoBalls x="478" y="295" spacing="32" align="center" />
    <DemoMouse x="508" y="350" align="center" />
    <DemoClick x="458" y="362" align="center" />
    <DemoClear x="478" y="303" align="center" />

    <!-- DevTester Access Dialog Window (ID 999) -->
    <DevDialog width="380" minTop="10" align="center" />
</DevHelpScreen>
```

#### Detailed Element Specifications:
* **`HeaderTitle`**: Coordinates and anchor for the main "DEVELOPER MODE" title rendered with PopCap's Sandy Aztec Grain Glow.
* **`HeaderSubtitle`**: Position and anchor for the gold subtitle text ("Special Testing Profile - Unrestricted Game Access").
* **`Columns`**: Defines the horizontal centers for Column 1 (`col1X`, left side for Adventure Mode & Cheats) and Column 2 (`col2X`, right side for Gauntlet Mode & Demo Animation).
* **`AdvHeader`**: Vertical position and anchor for the yellow "ADVENTURE MODE" section heading.
* **`Temple5Preview`**: Coordinates and anchor for the glowing Secret Temple 5 preview graphic (190x75 px) displaying the green doorway to Stage 13.
* **`AdvBullets`**: Origin coordinates (`x`, `y`), vertical line interval (`spacing`), and alignment anchor for the Adventure Mode feature bullet points.
* **`GauntHeader`**: Vertical position and anchor for the yellow "GAUNTLET MODE" section heading.
* **`GauntBullets`**: Origin coordinates (`x`, `y`), vertical line interval (`spacing`), and alignment anchor for the Gauntlet Mode Sun God rank bullet points.
* **`CheatHeader`**: Vertical position and anchor for the cyan "MIDDLE MOUSE BUTTON" cheat feature header.
* **`CheatBullets`**: Origin coordinates (`x`, `y`), vertical line interval (`spacing`), and alignment anchor for the 5 cheat instruction lines explaining instant stage completion and DestroyAll marble explosions.
* **`DemoBalls`**: Centroid (`x`, `y`), inter-ball spacing (`spacing`), and alignment anchor for the 5-ball live animated demonstration.
* **`DemoMouse`**: Coordinates and alignment anchor for the animated mouse icon (`IMAGE_TUT_MOUSE_M`) illustrating middle-click activation.
* **`DemoClick`**: Position and alignment anchor for the white "CLICK!" status indicator text that illuminates during button press.
* **`DemoClear`**: Coordinates and anchor for the celebratory "STAGE CLEARED!" golden banner displayed when marbles disintegrate.
* **`DevDialog`**: Width (`width`), minimum top margin (`minTop`), and spatial alignment anchor (`align`) for the modal Developer Access dialog (ID 999) displayed when activating the DevTester profile.

---

## Modding Recipes

### Recipe 1: Customizing Secret Temple 5 and Doorway
To move Temple 5 to the top-center of the map with the frog standing directly before it:
```xml
<AdventureTemples>
    <Temple5 x="126" y="100" />
</AdventureTemples>
<AdventureDoors>
    <Door id="12" doorX="291" doorY="128" frogX="323" frogY="160" />
</AdventureDoors>
```

### Recipe 2: Repositioning In-Game Menubar for Ultra-Wide Displays
To space the Menubar widgets farther apart on a 21:9 ultrawide monitor:
```xml
<InGameHUD>
    <Menubar livesX="40" stageCX="220" scoreRX="400" zumaX="480" menuX="600" />
    <MenubarMargins top="40" bottom="80" left="150" right="120" />
</InGameHUD>
```

### Recipe 3: Shifting In-Game HUD for Stream Overlays
If you are streaming and need the level banner ("LEVEL 1-1") to sit higher to avoid a webcam overlay:
```xml
<InGameHUD>
    <LevelBanner offsetX="50" offsetY="100" />
</InGameHUD>
```

---

## Automated Generation & Self-Healing Architecture

ZuCompleluxe features a fully automated asset synchronization engine implemented inside `builder/patch_assets.py`. Modders and players never need to manually copy or template `layout.xml`:

1. **Automatic Fresh Generation**: When running `build_full.bat` or `build_lite.bat` on a clean Zuma installation, the patch engine automatically constructs `properties/layout.xml` in the installation directory with all 17 configuration sections pre-populated.
2. **Non-Destructive Intelligent Upgrades**: If an existing `properties/layout.xml` is present, the engine inspects its contents. If older layouts are missing newly introduced sections (such as `<DevHelpScreen>`, `<Dialogs>`, or `<Menubar>`), the patch engine automatically upgrades the file to include the latest elements while safely preserving existing user adjustments.
3. **Full Edition & Lite Edition Parity**: Both `ZuCompleluxe` (Full) and `ZuCompleluxe-lite` (Lite) share the identical layout parsing engine (`zrh_layout.c`), data structures (`zrh_layout.h`), and automated generation rules, ensuring 100% cross-edition parity.

---

## Error Handling & Safe Fallback Architecture

`zumareshandler.dll` and `zumatestingkit.dll` incorporate multiple layers of validation to prevent crashes:
1. **Missing File Fallback**: If `properties/layout.xml` is deleted or inaccessible, the engine logs a warning to `zrh_log.txt` and automatically falls back to internal default vanilla coordinates.
2. **Missing Section / Attribute Fallback**: Any tag or attribute omitted from `layout.xml` retains its built-in default value. Modders only need to declare the specific elements they wish to customize.
3. **PE Memory Safety**: All memory patches applied to the game executable use atomic `VirtualProtect` read-write transitions with byte-pattern verification. If a pattern signature mismatches, patching aborts cleanly without corrupting neighboring instructions.
