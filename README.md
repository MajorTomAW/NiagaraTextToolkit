# Niagara Text Toolkit (NTT)

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

