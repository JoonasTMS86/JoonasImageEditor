# JoonasImageEditor
Joonas DOS Game Development Tools - The Image Editor

2025 Joonas Lindberg

With this image editor, you can draw 256-color VGA images which you can use in your own MS-DOS game.

Left click a color in the color palette to choose the color for brush 1 (left mouse button brush).
Right click a color in the color palette to choose the color for brush 2 (right mouse button brush).

You can edit the RGB value of the selected color by sliding the three sliders below the drawing area.
VGA RGB values can be in the range 0 ... 63.

By clicking the "File -> Open" option, you can load any file that's recognized by The Image Editor.

Recognized file formats:

.VGA: 64,000-byte 320 x 200 VGA picture file without image size and palette info

.PAL: 768-byte VGA palette file

.IMG: 64,768-byte 320 x 200 VGA picture file without image size and with palette info (found at the end of the file)

.PIC: VGA image file. The 4-byte header determines the width & height of the image. The first 2 bytes indicate the width, the next 2 bytes the height.


By clicking the "File -> Save As..." option, you can save your image, palette or image and palette to any of the above formats.
Simply add the file extension to the filename and it will be saved in the desired format.

The Image Editor is still a Work-In-Progress. I will refactor the code and add many new features to the tool later.

- Joonas
