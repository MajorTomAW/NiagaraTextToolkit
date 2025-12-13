# Niagara Text Toolkit (NTT)

## Table of Contents

- [Introduction](#introduction)
- [Installation](#installation)
- [How It Works](#how-it-works)
- [Recommended Niagara Emitter Settings](#recommended-niagara-emitter-settings)
- [Adding Custom Fonts](#adding-custom-fonts)
  - [Creating a Font Asset](#creating-a-font-asset)
  - [Configuring Character Set](#configuring-character-set)
  - [Optimizing Texture Layout](#optimizing-texture-layout)
  - [Generating Distance Field Texture](#generating-distance-field-texture)
  - [Extracting the Texture Asset](#extracting-the-texture-asset)
  - [Using Your Custom Font](#using-your-custom-font)
- [Documentation](#documentation)
  - [NTT Data Interface](#ntt-data-interface)
    - [Properties](#properties)
    - [Exposed Functions (Niagara)](#exposed-functions-niagara)
  - [Blueprint Library](#blueprint-library)
  - [Editor Utilities](#editor-utilities)

## Introduction

The **Niagara Text Toolkit (NTT)** is a plugin for Unreal Engine that aims to simplify the process of creating text based particle systems.

The plugin provides a custom **Niagara Data Interface (NTT Data Interface)** that parses text strings and font data to generate attributes (UVs, positions, sizes) that can be assigned to particles in Niagara Scratch Modules. This allows you to display text with the full performance benifits of Niagara particle systems while being flexible and easy to use. The plugin also supports custom layout options (alignment, kerning, line spacing, word spacing), and automatically filters out whitespace from the input text.

## Installation

1. Close your Unreal Engine project.
2. Copy the `NiagaraTextToolkit` folder into the `Plugins` directory of your project. 
   - If a `Plugins` folder does not exist in your project root, create one.
   - Path example: `[YourProjectRoot]/Plugins/NiagaraTextToolkit/`
3. Open your project. Unreal Engine may ask to rebuild the plugin modules; select **Yes**.
4. Once the editor is open, verify the plugin is enabled by going to **Edit > Plugins** and searching for "Niagara Text Toolkit".

## How It Works

Niagara simulations are inherently less flexible than regular game-thread code because they rely on GPU/CPU parallelization and tight data packing for optimization. The benefit is that they’re extremely fast for certain use cases, but the downside is that parallel execution limits the kinds of algorithms you can implement, and the data packing pushes you toward data structures that “play nice” with Niagara.

For text-based particle systems, these limitations show up in a few ways:

First, the data problem. You want to display an input string, but Niagara doesn’t take a string as input. So the first question is: how do you encode your text into a Niagara-friendly data type? One solution is to pass your input as an array of integers, where each element is the Unicode value for a character. This works, but you have to build that array on the game thread before you spawn the system. The Lyra starter game’s “DamageNumbers” takes a different approach: it passes a float and uses math to derive the number of particles from the number of digits. That’s a clever workaround, but it only works for numbers, not a generic string. So if you want arbitrary text, you end up needing some amount of game-thread preprocessing before the Niagara simulation starts.

Next is positioning. To position a character properly relative to other characters, you need to know the widths of the characters that came before it, and if you want centered text you also need to know the width of the whole line. That implies (at minimum) two passes over the input: one to compute line widths, and one to compute per-character positions. The problem is you can’t really “do two passes” in Niagara; the simulation just doesn’t work like that. Your options are to precompute on the game thread, or restrict yourself (for example) to monospaced fonts where line width is just `CharacterWidth * CharacterCount`. If you want to support any font (not just monospaced), you need some form of iteration over the input on the game thread.

Then there’s the glyph/UV problem. Even if Niagara can spawn and position particles, each particle still needs to display the right glyph from a texture atlas. Usually that means sampling a subset of a font atlas by applying a per-character UV offset and scale in the material, and you also need a way to communicate “which character am I?” from Niagara to the material. Lyra’s number approach can compute UVs in-material because the digit value doubles as a glyph index, but that breaks down as soon as you want characters that aren’t digits.

So overall, building text in Niagara isn’t as straightforward as it looks: you either limit the scope (like the Lyra example), or you accept that some work needs to happen on the game thread and feed Niagara data that it can consume efficiently.

This plugin takes the “no compromise” approach by doing the game-thread work once up-front and exposing the results to Niagara in a Niagara-friendly way.

The first key insight is to use Unreal Engine Font Assets. The benefit is you don’t need to manually author textures (the font asset generates them), and you don’t need to hand-calculate per-character UVs (the font asset already has that information).

Early versions of this plugin exposed this via a Blueprint library: extract the font texture and character data, convert strings to Unicode arrays, compute character positions, then pass all of that into the Niagara system. It worked, but it was clunky, and Unreal has a better mechanism for this.

Enter Data Interfaces.

In Niagara, Data Interfaces are used to interface data between the game thread and your Niagara simulation. They have an initialization phase that runs once when the system is initialized, where you can process game-thread data and pack it into structures Niagara likes. They also let you define functions that Niagara can call during simulation to retrieve that packed data. (If you want the deep details, feel free to read the source.)

Using a Data Interface for text solves a lot of problems in one fell swoop:

- You can assign a **Font Asset** directly in Niagara.
- You can type an **Input Text** string directly in Niagara (no manual Unicode array setup).
- The Data Interface can do the per-string/per-font preprocessing during initialization, and Niagara can query the results via functions like UV/position/size lookups.

At a high level, the workflow looks like this:

1. Create an Offline Font Asset (see the section below).
2. Add the NTT Data Interface to your Niagara System as a User Parameter.
3. Set `Font Asset` and `Input Text` on the Data Interface.
4. In your Niagara modules, call the exposed NTT functions (position/UV/size/counts) to drive per-particle attributes and render the correct glyphs.

## Recommended Niagara Emitter Settings

The following are recommended settings on Niagara emitters.

First of all, **Persistent IDs** must be enabled to avoid undefined behavior. The Data Interface assumes that the number of particles spawned by the emitter matches the length of the input text (ignoring whitespace if enabled). The plugin assumes you’ll use the `UniqueID` particle property to index each character, so that value needs to be persistent to avoid bugs.

Next, you should enable **Local Space** simulation on your emitters. This isn’t required for the plugin to work, but emitters default to world space, and if you forget to switch to local space your particles will spawn at the world origin instead of where you spawn the Niagara System.

Finally, I recommend using **CPU simulations**. GPU simulations are fully supported (the Data Interface functions are implemented for GPU too), but there are two main downsides for typical text use cases:

- The particle counts are usually low enough that GPU parallelization doesn’t provide much benefit, and the cost of uploading data can outweigh the marginal compute savings.
- Right now the Data Interface uploads data to the GPU every frame instead of only during initialization. Fixing this is on my to-do list; I had a solution, but it relied on Niagara APIs that change across engine versions, so to keep things simple and compatible across versions I removed it.

## Adding Custom Fonts

To use custom fonts with the Niagara Text Toolkit, you need to create and configure a font asset in Unreal Engine.

### Creating a Font Asset

1. In the Content Browser, right-click and select **New > Font Asset**.
2. Change the font mode to **Offline**. This makes the font asset generate a texture instead of using vector fonts.
3. Select a font from the fonts installed on your operating system.

### Configuring Character Set

1. In the font asset, expand the **Import Options** section.
2. Scroll down to the **Chars** section, which lets you input the characters you want included.
3. Enter the whole alphabet and any numbers and symbols you need. **Remember to also add uppercase characters.**

   Example character set:
   ```
   abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*()-_=+/?.>,<'";:
   ```

   **Important:** If you don't explicitly specify the characters you want, you'll have a lot of characters included in the texture that you won't need. You want to maximize the resolution of the characters that are actually going to be displayed.

### Optimizing Texture Layout

1. Right-click on the font asset in your Content Browser and select **Reimport**. This will apply the changes you made to the import settings.
2. Open your font asset and adjust the **Height** property so that all characters are displayed on a single texture page.
   - **Increase** the height if there's a lot of unused area.
   - **Decrease** it if the characters overflow to a second page.
3. Keep adjusting and reimporting until you're satisfied with the layout.

### Generating Distance Field Texture

1. Toggle the **Use Distance Field Alpha** property in the font asset.
2. Reimport one final time. This will generate a distance field texture which is necessary for features like text borders. The materials included with the plugin expect an SDF (Signed Distance Field) texture.

### Extracting the Texture Asset

1. Right-click on the font asset in the Content Browser.
2. Go to **Scripted Asset Actions** and select **Save Font Textures To Assets**.
3. This will create a standalone Texture2D asset that you can use in materials.

### Using Your Custom Font

1. Add the font asset to the **Font Asset** property in the NTT Data Interface in your Niagara System.
2. Add the extracted texture to your material parameter collection or material instance.

## Documentation

### NTT Data Interface

The core feature of this plugin is the `NTT Data Interface`, which can be added to your Niagara System as a User Parameter.

#### Properties

| Property | Description |
| --- | --- |
| **Font Asset** | The `UFont` asset used for layout and UVs. **Note:** This must be an "Offline" font (legacy cache type) to provide the necessary texture data to Niagara. You also need to make sure all the characters you want are on a single page|
| **Input Text** | The string of text to generate. Supports multi-line strings. |
| **Horizontal Alignment** | Aligns the text layout: `Left`, `Center`, or `Right`. |
| **Vertical Alignment** | Aligns the text layout: `Top`, `Center`, or `Bottom`. |
| **Vertical Offset** | Controls the vertical spacing between lines. |
| **Kerning Offset** | Adds additional spacing between characters (kerning). |
| **Whitespace Width Multiplier** | Multiplies the width of whitespace characters (useful for adjusting word spacing). |
| **Filter Whitespace Characters** | If enabled, whitespace characters are excluded from the list of valid particle positions (prevents spawning invisible particles). |

#### Exposed Functions (Niagara)

These functions are available within Niagara Modules (Scratch Pad or Script) when using the NTT Data Interface.

- **GetCharacterUV**
  - *Inputs*: `CharacterIndex` (int)
  - *Outputs*: `USize`, `VSize`, `UStart`, `VStart` (floats)
  - *Description*: Returns the UV rectangle coordinates for the glyph at the specified index.

- **GetCharacterPosition**
  - *Inputs*: `CharacterIndex` (int)
  - *Outputs*: `CharacterPosition` (Vector)
  - *Description*: Returns the local position of the character relative to the text alignment origin.

- **GetCharacterSpriteSize**
  - *Inputs*: `CharacterIndex` (int)
  - *Outputs*: `SpriteSize` (Vector2D)
  - *Description*: Returns the original pixel dimensions (width, height) of the glyph.

- **GetTextCharacterCount**
  - *Outputs*: `CharacterCount` (int)
  - *Description*: Returns the total number of characters in the `InputText`.

- **GetTextLineCount**
  - *Outputs*: `LineCount` (int)
  - *Description*: Returns the total number of lines.

- **GetTextWordCount**
  - *Outputs*: `WordCount` (int)
  - *Description*: Returns the total number of words.

- **GetLineCharacterCount**
  - *Inputs*: `LineIndex` (int)
  - *Outputs*: `LineCharacterCount` (int)
  - *Description*: Returns the number of characters in the specified line.

- **GetWordCharacterCount**
  - *Inputs*: `WordIndex` (int)
  - *Outputs*: `WordCharacterCount` (int)
  - *Description*: Returns the number of characters in the specified word.

- **GetWordTrailingWhitespaceCount**
  - *Inputs*: `WordIndex` (int)
  - *Outputs*: `TrailingWhitespaceCount` (int)
  - *Description*: Returns the number of whitespace characters immediately following the specified word.

- **GetCharacterCountInLineRange**
  - *Inputs*: `StartLineIndex` (int), `EndLineIndex` (int)
  - *Outputs*: `CharacterCountInLineRange` (int)
  - *Description*: Returns the total number of characters across a range of lines.

- **GetCharacterCountInWordRange**
  - *Inputs*: `StartWordIndex` (int), `EndWordIndex` (int)
  - *Outputs*: `CharacterCountInRange` (int)
  - *Description*: Returns the total number of characters across a range of words.

- **GetTextHeight**
  - *Outputs*: `TextHeight` (float)
  - *Description*: Returns the total vertical height of the generated text block.

- **GetFilterWhitespaceCharacters**
  - *Outputs*: `FilterWhitespaceCharacters` (bool)
  - *Description*: Returns the current state of the whitespace filter setting.

### Blueprint Library

The plugin includes the `NiagaraTextToolkitHelpers` library for controlling the system at runtime via Blueprints.

- **Set Niagara NTT Text Variable**
  - *Inputs*: `NiagaraSystem` (Niagara Component), `TextToDisplay` (String)
  - *Description*: Updates the `InputText` variable on the NTT Data Interface of the target Niagara Component and reinitializes the system.

- **Set Niagara NTT Font Variable**
  - *Inputs*: `NiagaraSystem` (Niagara Component), `Font` (UFont)
  - *Description*: Updates the `FontAsset` variable on the NTT Data Interface of the target Niagara Component and reinitializes the system.

### Editor Utilities

- **Save Font Textures To Assets**
  - *Type*: Editor Utility (Scripted Asset Action)
  - *Description*: A helper utility to extract textures from an Offline Font and save them as standalone Texture2D assets. This is useful for sampling font textures in materials.

