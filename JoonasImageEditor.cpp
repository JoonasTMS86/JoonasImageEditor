/*
Use this to compile:
g++ --std=c++17 JoonasImageEditor.cpp -o JoonasImageEditor -W -Wall -pedantic `pkg-config gtkmm-3.0 --cflags --libs`

File formats in my image editor:
.VGA: 64,000-byte 320 x 200 VGA picture file without image size and palette info
.PAL: 768-byte VGA palette file
.IMG: 64,768-byte 320 x 200 VGA picture file without image size and with palette info (found at the end of the file)
.PIC: VGA image file. The 4-byte header determines the width & height of the image. The first 2 bytes indicate the width, the next 2 bytes the height.

As VGA RGB values can be in the range 0 ... 63, that means that each VGA palette entry uses 6 bits per color value.
If I want to make my palette files even smaller in the future, I could make use of the 6-bit thing so that
each VGA palette color takes 3 * 6 bits = 18 bits per VGA palette color, 18 bits * 256 = 4608 bits = 576 bytes
*/

#include <gtk/gtk.h>
#include <gtkmm.h>
#include <gtkmm/application.h>
#include <iostream>
#include <fstream>
#include <cstdlib>

#define pixel_size 2 // Size (width and height) of each pixel of the VGA screen
#define drawingAreaWidth 700
#define drawingAreaHeight 200 * pixel_size
#define drawingAreaRowStride drawingAreaWidth * 3
#define drawingAreaImageSize drawingAreaWidth * 3 * drawingAreaHeight
#define size_of_palette_square 10 // Size of each selectable color of the color palette
#define palette_square_cols 64 // How many on one line
#define palette_square_rows 4 // How many rows
#define palette_toolbar_height size_of_palette_square * palette_square_rows
#define palette_toolbar_bitmap_size drawingAreaRowStride * (size_of_palette_square * palette_square_rows)
#define size_of_interaction_window drawingAreaImageSize + palette_toolbar_bitmap_size
#define FILE_EXTENSION_PAL 1
#define FILE_EXTENSION_VGA 2
#define FILE_EXTENSION_PIC 3
#define gtk_menu_append(menu,child) gtk_menu_shell_append  ((GtkMenuShell *)(menu),(child))
#define DISABLED_AREA_OF_DRAWINGAREA_COLOR_R 78
#define DISABLED_AREA_OF_DRAWINGAREA_COLOR_G 78
#define DISABLED_AREA_OF_DRAWINGAREA_COLOR_B 60

char * loadedFile = (char*) malloc(256000); // Loaded 320 x 200 image, which can be eg. BMP, PNG, JPG or a 256-color VGA picture file which uses my own file extension and file format.
char * loadedPalette = (char*) malloc(768);
unsigned int loadedSize = 64000; // Upon starting the program, the image is a blank 320x200 VGA picture, which makes the filesize of the file to save 64000 bytes.

static cairo_surface_t *surface = NULL;

GtkWidget *da;
GtkWidget *slider, *slider2, *slider3;
GtkWidget *widthField;
GtkWidget *heightField;
GtkWidget *sourceColorField;
GtkWidget *targetColorField;

GdkPixbuf *pixbuf;

guchar data[size_of_interaction_window];

unsigned char VGA_screen[64000]; // The 320x200 VGA screen that is used to draw on.

/*
 The palette registers array should contain RGB colors in the 8-bit BGR format (R, G and B can have the value 0 ... 255).
 Remember that when saving a .PAL palette file, the saved values should be in the VGA 6-bit RGB format (R, G and B can have the value 0 ... 63).
 Likewise, when loading a .PAL palette file, the VGA 6-bit RGB values in it should be converted to the 8-bit RGB format.
*/
int VGA_palette_registers[768] =
{
0x00,0x00,0x00,0x00,0x00,0xAA,0x00,0xAA,0x00,0x00,0xAA,0xAA,0xAA,0x00,0x00,0xAA,0x00,0xAA,0xAA,0x55,0x00,0xAA,0xAA,0xAA,0x55,0x55,0x55,0x55,0x55,0xFF,0x55,0xFF,0x55,0x55,0xFF,0xFF,0xFF,0x55,0x55,0xFF,0x55,0xFF,0xFF,0xFF,0x55,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x14,0x14,0x14,0x20,0x20,0x20,0x2C,0x2C,0x2C,0x38,0x38,0x38,0x45,0x45,0x45,0x51,0x51,0x51,0x61,0x61,0x61,0x71,0x71,0x71,0x82,0x82,0x82,0x92,0x92,0x92,0xA2,0xA2,0xA2,0xB6,0xB6,0xB6,0xCB,0xCB,0xCB,0xE3,0xE3,0xE3,0xFF,0xFF,0xFF,0x00,0x00,0xFF,0x41,0x00,0xFF,0x7D,0x00,0xFF,0xBE,0x00,0xFF,0xFF,0x00,0xFF,0xFF,0x00,0xBE,0xFF,0x00,0x7D,0xFF,0x00,0x41,0xFF,0x00,0x00,0xFF,0x41,0x00,0xFF,0x7D,0x00,0xFF,0xBE,0x00,0xFF,0xFF,0x00,0xBE,0xFF,0x00,0x7D,0xFF,0x00,0x41,0xFF,0x00,0x00,0xFF,0x00,0x00,0xFF,0x41,0x00,0xFF,0x7D,0x00,0xFF,0xBE,0x00,0xFF,0xFF,0x00,0xBE,0xFF,0x00,0x7D,0xFF,0x00,0x41,0xFF,0x7D,0x7D,0xFF,0x9E,0x7D,0xFF,0xBE,0x7D,0xFF,0xDF,0x7D,0xFF,0xFF,0x7D,0xFF,0xFF,0x7D,0xDF,0xFF,0x7D,0xBE,0xFF,0x7D,0x9E,0xFF,0x7D,0x7D,0xFF,0x9E,0x7D,0xFF,0xBE,0x7D,0xFF,0xDF,0x7D,0xFF,0xFF,0x7D,0xDF,0xFF,0x7D,0xBE,0xFF,0x7D,0x9E,0xFF,0x7D,0x7D,0xFF,0x7D,0x7D,0xFF,0x9E,0x7D,0xFF,0xBE,0x7D,0xFF,0xDF,0x7D,0xFF,0xFF,0x7D,0xDF,0xFF,0x7D,0xBE,0xFF,0x7D,0x9E,0xFF,0xB6,0xB6,0xFF,0xC7,0xB6,0xFF,0xDB,0xB6,0xFF,0xEB,0xB6,0xFF,0xFF,0xB6,0xFF,0xFF,0xB6,0xEB,0xFF,0xB6,0xDB,0xFF,0xB6,0xC7,0xFF,0xB6,0xB6,0xFF,0xC7,0xB6,0xFF,0xDB,0xB6,0xFF,0xEB,0xB6,0xFF,0xFF,0xB6,0xEB,0xFF,0xB6,0xDB,0xFF,0xB6,0xC7,0xFF,0xB6,0xB6,0xFF,0xB6,0xB6,0xFF,0xC7,0xB6,0xFF,0xDB,0xB6,0xFF,0xEB,0xB6,0xFF,0xFF,0xB6,0xEB,0xFF,0xB6,0xDB,0xFF,0xB6,0xC7,0xFF,0x00,0x00,0x71,0x1C,0x00,0x71,0x38,0x00,0x71,0x55,0x00,0x71,0x71,0x00,0x71,0x71,0x00,0x55,0x71,0x00,0x38,0x71,0x00,0x1C,0x71,0x00,0x00,0x71,0x1C,0x00,0x71,0x38,0x00,0x71,0x55,0x00,0x71,0x71,0x00,0x55,0x71,0x00,0x38,0x71,0x00,0x1C,0x71,0x00,0x00,0x71,0x00,0x00,0x71,0x1C,0x00,0x71,0x38,0x00,0x71,0x55,0x00,0x71,0x71,0x00,0x55,0x71,0x00,0x38,0x71,0x00,0x1C,0x71,0x38,0x38,0x71,0x45,0x38,0x71,0x55,0x38,0x71,0x61,0x38,0x71,0x71,0x38,0x71,0x71,0x38,0x61,0x71,0x38,0x55,0x71,0x38,0x45,0x71,0x38,0x38,0x71,0x45,0x38,0x71,0x55,0x38,0x71,0x61,0x38,0x71,0x71,0x38,0x61,0x71,0x38,0x55,0x71,0x38,0x45,0x71,0x38,0x38,0x71,0x38,0x38,0x71,0x45,0x38,0x71,0x55,0x38,0x71,0x61,0x38,0x71,0x71,0x38,0x61,0x71,0x38,0x55,0x71,0x38,0x45,0x71,0x51,0x51,0x71,0x59,0x51,0x71,0x61,0x51,0x71,0x69,0x51,0x71,0x71,0x51,0x71,0x71,0x51,0x69,0x71,0x51,0x61,0x71,0x51,0x59,0x71,0x51,0x51,0x71,0x59,0x51,0x71,0x61,0x51,0x71,0x69,0x51,0x71,0x71,0x51,0x69,0x71,0x51,0x61,0x71,0x51,0x59,0x71,0x51,0x51,0x71,0x51,0x51,0x71,0x59,0x51,0x71,0x61,0x51,0x71,0x69,0x51,0x71,0x71,0x51,0x69,0x71,0x51,0x61,0x71,0x51,0x59,0x71,0x00,0x00,0x41,0x10,0x00,0x41,0x20,0x00,0x41,0x30,0x00,0x41,0x41,0x00,0x41,0x41,0x00,0x30,0x41,0x00,0x20,0x41,0x00,0x10,0x41,0x00,0x00,0x41,0x10,0x00,0x41,0x20,0x00,0x41,0x30,0x00,0x41,0x41,0x00,0x30,0x41,0x00,0x20,0x41,0x00,0x10,0x41,0x00,0x00,0x41,0x00,0x00,0x41,0x10,0x00,0x41,0x20,0x00,0x41,0x30,0x00,0x41,0x41,0x00,0x30,0x41,0x00,0x20,0x41,0x00,0x10,0x41,0x20,0x20,0x41,0x28,0x20,0x41,0x30,0x20,0x41,0x38,0x20,0x41,0x41,0x20,0x41,0x41,0x20,0x38,0x41,0x20,0x30,0x41,0x20,0x28,0x41,0x20,0x20,0x41,0x28,0x20,0x41,0x30,0x20,0x41,0x38,0x20,0x41,0x41,0x20,0x38,0x41,0x20,0x30,0x41,0x20,0x28,0x41,0x20,0x20,0x41,0x20,0x20,0x41,0x28,0x20,0x41,0x30,0x20,0x41,0x38,0x20,0x41,0x41,0x20,0x38,0x41,0x20,0x30,0x41,0x20,0x28,0x41,0x2C,0x2C,0x41,0x30,0x2C,0x41,0x34,0x2C,0x41,0x3C,0x2C,0x41,0x41,0x2C,0x41,0x41,0x2C,0x3C,0x41,0x2C,0x34,0x41,0x2C,0x30,0x41,0x2C,0x2C,0x41,0x30,0x2C,0x41,0x34,0x2C,0x41,0x3C,0x2C,0x41,0x41,0x2C,0x3C,0x41,0x2C,0x34,0x41,0x2C,0x30,0x41,0x2C,0x2C,0x41,0x2C,0x2C,0x41,0x30,0x2C,0x41,0x34,0x2C,0x41,0x3C,0x2C,0x41,0x41,0x2C,0x3C,0x41,0x2C,0x34,0x41,0x2C,0x30,0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

int brush1_color = 1; // VGA palette index value of currently selected color for brush 1 (left mouse button)
int brush2_color = 2; // VGA palette index value of currently selected color for brush 2 (right mouse button)
int imageWidth = 320; // Current width of image
int imageHeight = 200; // Current height of image

void put_palette_square_to_screen(int x, int y, int VGA_palette_index_color) {
	guchar palette_square_gfx[size_of_palette_square * 3 * size_of_palette_square] = {
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,
	0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F,0x7F
	};
	bool palette_square_isNotTransparentPixel[size_of_palette_square * 3 * size_of_palette_square] = {
	true,true,true,true,true,true,true,true,true,true,
	true,true,true,true,true,true,true,true,true,true,
	true,true,false,false,false,false,false,false,true,true,
	true,true,false,false,false,false,false,false,true,true,
	true,true,false,false,false,false,false,false,true,true,
	true,true,false,false,false,false,false,false,true,true,
	true,true,false,false,false,false,false,false,true,true,
	true,true,false,false,false,false,false,false,true,true,
	true,true,true,true,true,true,true,true,true,true,
	true,true,true,true,true,true,true,true,true,true,
	};
	int valueR = VGA_palette_registers[(VGA_palette_index_color * 3) + 0];
	int valueG = VGA_palette_registers[(VGA_palette_index_color * 3) + 1];
	int valueB = VGA_palette_registers[(VGA_palette_index_color * 3) + 2];
	int pos = (y * drawingAreaRowStride) + (x * 3);
	for(int y = 0; y < size_of_palette_square; y++) {
		for(int x = 0; x < size_of_palette_square; x++) {
			if(palette_square_isNotTransparentPixel[(y * size_of_palette_square) + x]) {
				data[pos + (y * drawingAreaRowStride) + (x * 3) + 0] = palette_square_gfx[(y * size_of_palette_square * 3) + (x * 3) + 0];
				data[pos + (y * drawingAreaRowStride) + (x * 3) + 1] = palette_square_gfx[(y * size_of_palette_square * 3) + (x * 3) + 1];
				data[pos + (y * drawingAreaRowStride) + (x * 3) + 2] = palette_square_gfx[(y * size_of_palette_square * 3) + (x * 3) + 2];
			}
			else {
				data[pos + (y * drawingAreaRowStride) + (x * 3) + 0] = valueR;
				data[pos + (y * drawingAreaRowStride) + (x * 3) + 1] = valueG;
				data[pos + (y * drawingAreaRowStride) + (x * 3) + 2] = valueB;
			}
		}
	}
}

void refresh_currently_selected_colors() {
	put_palette_square_to_screen(657, drawingAreaHeight + 7, brush2_color);
	put_palette_square_to_screen(650, drawingAreaHeight, brush1_color);
	gtk_widget_queue_draw_area (da, 650, drawingAreaHeight, size_of_palette_square * 2, size_of_palette_square * 2);
}

void create_palette_toolbar() {
	int index_pos = 0;
	for(int y = 0; y < palette_square_rows; y++)
	{
		for(int x = 0; x < palette_square_cols; x++)
		{
			put_palette_square_to_screen((x * size_of_palette_square), (drawingAreaHeight + (y * size_of_palette_square)), index_pos);
			index_pos++;
		}
	}
	refresh_currently_selected_colors();
}

static void
clear_surface (void)
{
	cairo_t *cr;

	cr = cairo_create (surface);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint (cr);

	cairo_destroy (cr);
}

static gboolean
configure_event_cb (GtkWidget         *widget,
                    GdkEventConfigure *event,
                    gpointer           data)
{
	if (surface)
		cairo_surface_destroy (surface);

	surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
	CAIRO_CONTENT_COLOR,
	gtk_widget_get_allocated_width (widget),
	gtk_widget_get_allocated_height (widget));

	clear_surface ();

	return TRUE;
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   data)
{
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
	cairo_paint (cr);

	return FALSE;
}

static void
close_window (void)
{

	std::cout << "Quitting program." << std::endl;
	free(loadedFile);
	free(loadedPalette);

	if (surface)
		cairo_surface_destroy (surface);

	gtk_main_quit ();
}

void put_vga_picture_to_screen() {
	int xpos = 0;
	int ypos = 0;
	for(int pos = 0; pos < 64000; pos++)
	{
		int VGA_palette_index = VGA_screen[pos];
		if(VGA_palette_index < 0) VGA_palette_index += 256;
		int Rvalue = VGA_palette_registers[(VGA_palette_index * 3) + 0];
		int Gvalue = VGA_palette_registers[(VGA_palette_index * 3) + 1];
		int Bvalue = VGA_palette_registers[(VGA_palette_index * 3) + 2];

		// Put the pixel to the image area.
		for(int squareY = 0; squareY < pixel_size; squareY++)
		{
			for(int squareX = 0; squareX < pixel_size; squareX++)
			{
				data[(ypos * pixel_size * drawingAreaRowStride) + (squareY * drawingAreaRowStride) + (xpos * pixel_size * 3) + (squareX * 3) + 0] = Rvalue;
				data[(ypos * pixel_size * drawingAreaRowStride) + (squareY * drawingAreaRowStride) + (xpos * pixel_size * 3) + (squareX * 3) + 1] = Gvalue;
				data[(ypos * pixel_size * drawingAreaRowStride) + (squareY * drawingAreaRowStride) + (xpos * pixel_size * 3) + (squareX * 3) + 2] = Bvalue;
			}
		}
		xpos++;
		if(xpos >= 320)
		{
			xpos = 0;
			ypos++;
		}
	}
	gtk_widget_queue_draw_area (da, 0, 0, drawingAreaWidth, drawingAreaHeight);
}

void put_pixel(int colorR, int colorG, int colorB, int x, int y) {
	int actualX = (x / pixel_size) * pixel_size;
	int actualY = (y / pixel_size) * pixel_size;
	int pos = (actualY * drawingAreaRowStride) + (actualX * 3);
	for(int ypos = 0; ypos < pixel_size; ypos++)
	{
		for(int xpos = 0; xpos < pixel_size; xpos++)
		{
			data[pos + (ypos * drawingAreaRowStride) + (xpos * 3) + 0] = colorR; // R
			data[pos + (ypos * drawingAreaRowStride) + (xpos * 3) + 1] = colorG; // G
			data[pos + (ypos * drawingAreaRowStride) + (xpos * 3) + 2] = colorB; // B
		}
	}
}

void draw_brush(int vga_pixel, int colorR, int colorG, int colorB, int x, int y) {
	int dpos = ((y / pixel_size) * 320) + (x / pixel_size);
	VGA_screen[dpos] = vga_pixel;

	put_pixel(colorR, colorG, colorB, x, y);
	int actualX = (x / pixel_size) * pixel_size;
	int actualY = (y / pixel_size) * pixel_size;
	gtk_widget_queue_draw_area (da, actualX, actualY, pixel_size, pixel_size);
}

static gboolean
motion_notify_event_cb (GtkWidget      *widget,
                        GdkEventMotion *event,
                        gpointer        data)
{
	if (surface == NULL)
		return FALSE;

	int brush_color;

	if(event->x < (320 * pixel_size) && event->y < (200 * pixel_size)) {
		if (event->state & GDK_BUTTON1_MASK) {
			brush_color = brush1_color;
			int colorR = VGA_palette_registers[(brush_color * 3) + 0];
			int colorG = VGA_palette_registers[(brush_color * 3) + 1];
			int colorB = VGA_palette_registers[(brush_color * 3) + 2];
			draw_brush (brush_color, colorR, colorG, colorB, event->x, event->y);
		}
		if (event->state & GDK_BUTTON3_MASK) {
			brush_color = brush2_color;
			int colorR = VGA_palette_registers[(brush_color * 3) + 0];
			int colorG = VGA_palette_registers[(brush_color * 3) + 1];
			int colorB = VGA_palette_registers[(brush_color * 3) + 2];
			draw_brush (brush_color, colorR, colorG, colorB, event->x, event->y);
		}
	}

	return TRUE;
}

static gboolean
button_press_event_cb (GtkWidget      *widget,
                       GdkEventButton *event,
                       gpointer        data)
{

	if (surface == NULL)
		return FALSE;

	int brush_color;

	bool left_click;
	if (event->button == GDK_BUTTON_PRIMARY) {
		brush_color = brush1_color;
		left_click = true;
	}
	if (event->button == GDK_BUTTON_SECONDARY) {
		brush_color = brush2_color;
		left_click = false;
	}

	int colorR = VGA_palette_registers[(brush_color * 3) + 0];
	int colorG = VGA_palette_registers[(brush_color * 3) + 1];
	int colorB = VGA_palette_registers[(brush_color * 3) + 2];

	if(event->x < (320 * pixel_size) && event->y < (200 * pixel_size)) {
		draw_brush (brush_color, colorR, colorG, colorB, event->x, event->y);
	}
	else {
		if(event->y >= (200 * pixel_size)) {
			int ypos = (event->y - (200 * pixel_size)) / size_of_palette_square;
			int color_index = (ypos * palette_square_cols) + (event->x / size_of_palette_square);
			if(left_click) {
				brush1_color = color_index;
			}
			else brush2_color = color_index;
			colorR = VGA_palette_registers[(color_index * 3) + 0];
			colorG = VGA_palette_registers[(color_index * 3) + 1];
			colorB = VGA_palette_registers[(color_index * 3) + 2];
			refresh_currently_selected_colors();
			if(left_click) {
				gtk_range_set_value(GTK_RANGE (slider), (colorR / 4));
				gtk_range_set_value(GTK_RANGE (slider2), (colorG / 4));
				gtk_range_set_value(GTK_RANGE (slider3), (colorB / 4));
			}
		}
	}

	return TRUE;
}

int getFileType(char *filename) {
	int pos = 0;
	while(filename[pos] != 0)
	{
		if(filename[pos] == '.')
		{
			if(filename[pos + 1] != 0 && filename[pos + 2] != 0 && filename[pos + 3] != 0)
			{
				// .PAL file extension?
				if((filename[pos + 1] == 'p' || filename[pos + 1] == 'P') && 
				(filename[pos + 2] == 'a' || filename[pos + 2] == 'A') && 
				(filename[pos + 3] == 'l' || filename[pos + 3] == 'L'))
				{
					return FILE_EXTENSION_PAL;
				}
				// .VGA file extension?
				if((filename[pos + 1] == 'v' || filename[pos + 1] == 'V') && 
				(filename[pos + 2] == 'g' || filename[pos + 2] == 'G') && 
				(filename[pos + 3] == 'a' || filename[pos + 3] == 'A'))
				{
					return FILE_EXTENSION_VGA;
				}
				// .PIC file extension?
				if((filename[pos + 1] == 'p' || filename[pos + 1] == 'P') && 
				(filename[pos + 2] == 'i' || filename[pos + 2] == 'I') && 
				(filename[pos + 3] == 'c' || filename[pos + 3] == 'C'))
				{
					return FILE_EXTENSION_PIC;
				}
			}
		}
		pos++;
	}
	return 0;
}

void set_size_of_drawingarea(int newWidth, int newHeight) {
	int widthOfDisabledArea = 320 - newWidth;
	int heightOfDisabledArea = 200 - newHeight;
	int x;
	int y = 0;

	put_vga_picture_to_screen();

	if(newWidth < 320) {
		for(int currHeight = 200; currHeight > 0; currHeight--) {
			x = newWidth * pixel_size;
			for(int currWidth = widthOfDisabledArea; currWidth > 0; currWidth--) {
				put_pixel(DISABLED_AREA_OF_DRAWINGAREA_COLOR_R, DISABLED_AREA_OF_DRAWINGAREA_COLOR_G, DISABLED_AREA_OF_DRAWINGAREA_COLOR_B, x, y);
				x += pixel_size;
			}
			y += pixel_size;
		}
	}

	if(newHeight < 200) {
		y = newHeight * pixel_size;
		for(int currHeight = heightOfDisabledArea; currHeight > 0; currHeight--) {
			x = 0;
			for(int currWidth = newWidth; currWidth > 0; currWidth--) {
				put_pixel(DISABLED_AREA_OF_DRAWINGAREA_COLOR_R, DISABLED_AREA_OF_DRAWINGAREA_COLOR_G, DISABLED_AREA_OF_DRAWINGAREA_COLOR_B, x, y);
				x += pixel_size;
			}
			y += pixel_size;
		}
	}
	imageWidth = newWidth;
	imageHeight = newHeight;
	loadedSize = imageWidth * imageHeight;
	gtk_widget_queue_draw_area (da, 0, 0, 320 * pixel_size, 200 * pixel_size);
}

void
menuitemclick (GtkMenuItem *menuitem) {

	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	gint res;

	dialog = gtk_file_chooser_dialog_new ("Open File",
		NULL,
		action,
		("_Cancel"),
		GTK_RESPONSE_CANCEL,
		("_Open"),
		GTK_RESPONSE_ACCEPT,
		NULL);

	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
		filename = gtk_file_chooser_get_filename (chooser);

		int fileType = getFileType(filename);

		std::ifstream sourcefile(filename, std::ios::in|std::ios::binary|std::ios::ate);
		if(sourcefile)
		{
			if(fileType == FILE_EXTENSION_PAL) {
				sourcefile.seekg (0, std::ios::beg);
				sourcefile.read (loadedPalette, 768);
			}
			else {
				loadedSize = sourcefile.tellg();
				sourcefile.seekg (0, std::ios::beg);
				sourcefile.read (loadedFile, loadedSize);
			}
		}
		else
		{
			std::cout << "Source file not found!" << std::endl;
		}
		sourcefile.close();

		if(fileType == FILE_EXTENSION_VGA) {
			for(int pos = 0; pos < 64000; pos++) {
				VGA_screen[pos] = loadedFile[pos];
			}
			imageWidth = 320;
			imageHeight = 200;
			std::cout << "Loaded 256-color VGA picture file." << std::endl;
			put_vga_picture_to_screen();
		}

		if(fileType == FILE_EXTENSION_PAL) {
			std::cout << "Loaded VGA palette file." << std::endl;
			for(int pos = 0; pos < 768; pos++)
			{
				int color = loadedPalette[pos];
				if(color < 0) color += 256;
				color *= 4;
				VGA_palette_registers[pos] = color;
			}
			// After loading the palette, we must update the colors of the image so that they correspond to the current VGA palette values.
			put_vga_picture_to_screen();
			create_palette_toolbar();
			int colorR = VGA_palette_registers[(brush1_color * 3) + 0];
			int colorG = VGA_palette_registers[(brush1_color * 3) + 1];
			int colorB = VGA_palette_registers[(brush1_color * 3) + 2];
			gtk_range_set_value(GTK_RANGE (slider), (colorR / 4));
			gtk_range_set_value(GTK_RANGE (slider2), (colorG / 4));
			gtk_range_set_value(GTK_RANGE (slider3), (colorB / 4));
		}

		if(fileType == FILE_EXTENSION_PIC) {
			int wb0 = loadedFile[0];
			int wb1 = loadedFile[1];
			int hb0 = loadedFile[2];
			int hb1 = loadedFile[3];
			if(wb0 < 0) wb0 += 256;
			if(wb1 < 0) wb1 += 256;
			if(hb0 < 0) hb0 += 256;
			if(hb1 < 0) hb1 += 256;
			int width = (wb1 * 256) + wb0;
			int height = (hb1 * 256) + hb0;
			for(int y = 0; y < height; y++) {
				for(int x = 0; x < width; x++) {
					VGA_screen[(y * 320) + x] = loadedFile[4 + (y * width) + x];
				}
			}
			std::cout << "Loaded 256-color VGA image with the size: " << width << "x" << height << std::endl;
			set_size_of_drawingarea(width, height);
		}
		g_free (filename);
	}
	gtk_widget_destroy (dialog);
}

void
menuitem2click (GtkMenuItem *menuitem) {
	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
	gint res;

	dialog = gtk_file_chooser_dialog_new ("Save File",
		NULL,
		action,
		("_Cancel"),
		GTK_RESPONSE_CANCEL,
		("_Save"),
		GTK_RESPONSE_ACCEPT,
		NULL);

	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
		filename = gtk_file_chooser_get_filename (chooser);

		int fileType = getFileType(filename);
		// .PAL file extension?
		if(fileType == FILE_EXTENSION_PAL) {
			std::cout << "saving pal file" << std::endl;
			for(int pos = 0; pos < 768; pos++)
			{
				int val = VGA_palette_registers[pos];
				if(val < 0) val += 256;
				val /= 4;
				loadedPalette[pos] = val;
			}
			std::ofstream savedfile (filename, std::ios::out|std::ios::binary|std::ios::ate);
			if (savedfile.is_open())
			{
				savedfile.write(loadedPalette, 768);
				savedfile.close();
			}
			else
			{
				std::cout << "Error creating file!" << std::endl;
			}
		}
		// .VGA file extension?
		if(fileType == FILE_EXTENSION_VGA) {
			std::cout << "saving vga file" << std::endl;
			std::ofstream savedfile (filename, std::ios::out|std::ios::binary|std::ios::ate);
			if (savedfile.is_open())
			{
				savedfile.write(loadedFile, 64000);
				savedfile.close();
			}
			else
			{
				std::cout << "Error creating file!" << std::endl;
			}
		}
		// .PIC file extension?
		if(fileType == FILE_EXTENSION_PIC) {
			std::cout << "saving pic file" << std::endl;
			int wb1 = imageWidth / 256;
			int wb0 = imageWidth - (wb1 * 256);
			int hb1 = imageHeight / 256;
			int hb0 = imageHeight - (hb1 * 256);
			std::cout << "wb0,wb1: " << wb0 << "," << wb1 << " hb0,hb1:" << hb0 << "," << hb1 << std::endl;
			loadedFile[0] = wb0;
			loadedFile[1] = wb1;
			loadedFile[2] = hb0;
			loadedFile[3] = hb1;
			for(int y = 0; y < imageHeight; y++) {
				for(int x = 0; x < imageWidth; x++) {
					loadedFile[4 + (y * imageWidth) + x] = VGA_screen[(y * 320) + x];
				}
			}
			loadedSize = (imageWidth * imageHeight) + 4;
			std::ofstream savedfile (filename, std::ios::out|std::ios::binary|std::ios::ate);
			if (savedfile.is_open())
			{
				savedfile.write(loadedFile, loadedSize);
				savedfile.close();
			}
			else
			{
				std::cout << "Error creating file!" << std::endl;
			}
		}
		g_free (filename);
	}
	gtk_widget_destroy (dialog);
}

/*
 Call this with one of three values to change the value of R, G or B.
 0 = change R
 1 = change G
 2 = change B
*/
void change_palette_of_selected_color(int indexOfRorGorB, int pos)
{
	VGA_palette_registers[(brush1_color * 3) + indexOfRorGorB] = pos;
	int pal_row = brush1_color / palette_square_cols;
	int pal_square_index = pal_row * palette_square_cols;
	int pal_square_x = (brush1_color - pal_square_index) * size_of_palette_square;
	int pal_square_y = (drawingAreaHeight + (pal_row * size_of_palette_square));
	put_palette_square_to_screen(pal_square_x, pal_square_y, brush1_color);
	gtk_widget_queue_draw_area (da, pal_square_x, pal_square_y, size_of_palette_square, size_of_palette_square);
	put_vga_picture_to_screen();
	refresh_currently_selected_colors();
}

void
slider_R_changed (GtkRange *range,
               gpointer  user_data)
{
	int pos;
	pos = gtk_range_get_value (GTK_RANGE (slider));
	pos *= 4;
	change_palette_of_selected_color(0, pos);
}
void
slider_G_changed (GtkRange *range,
               gpointer  user_data)
{
	int pos;
	pos = gtk_range_get_value (GTK_RANGE (slider2));
	pos *= 4;
	change_palette_of_selected_color(1, pos);
}
void
slider_B_changed (GtkRange *range,
               gpointer  user_data)
{
	int pos;
	pos = gtk_range_get_value (GTK_RANGE (slider3));
	pos *= 4;
	change_palette_of_selected_color(2, pos);
}

bool isNumeric(std::string givenText) {
	bool isValid = true;
	for(unsigned int pos = 0; pos < givenText.length(); pos++) {
		if(givenText[pos] < '0' || givenText[pos] > '9') {
			pos = givenText.length();
			isValid = false;
		}
	}
	return isValid;
}

void
widthField_changed (GtkEntry *entry,
               gpointer  user_data)
{
	std::string val = gtk_entry_get_text(GTK_ENTRY (widthField));
	bool valid = isNumeric(val);
	int newWidth;
	if(valid) {
		newWidth = stoi(val);
	}
	set_size_of_drawingarea(newWidth, imageHeight);
}

void
heightField_changed (GtkEntry *entry,
               gpointer  user_data)
{
	std::string val = gtk_entry_get_text(GTK_ENTRY (heightField));
	bool valid = isNumeric(val);
	int newHeight;
	if(valid) {
		newHeight = stoi(val);
	}
	set_size_of_drawingarea(imageWidth, newHeight);
}

void
sourceColorField_changed (GtkEntry *entry,
               gpointer  user_data)
{
	std::cout << "changed source color" << std::endl;
}

void
targetColorField_changed (GtkEntry *entry,
               gpointer  user_data)
{
	std::string sourceColorText = gtk_entry_get_text(GTK_ENTRY (sourceColorField));
	std::string targetColorText = gtk_entry_get_text(GTK_ENTRY (targetColorField));
	bool validSourceColor = isNumeric(sourceColorText);
	bool validTargetColor = isNumeric(targetColorText);
	if(validSourceColor && validTargetColor) {
		int sourceColor = stoi(sourceColorText);
		int targetColor = stoi(targetColorText);
		for(int pos = 0; pos < 64000; pos++) {
			if(VGA_screen[pos] == sourceColor) {
				VGA_screen[pos] = targetColor;
			}
		}
		put_vga_picture_to_screen();
	}
}

int
main (int   argc,
      char *argv[])
{
	for(int pos = 0; pos < size_of_interaction_window; pos++)
	{
		data[pos] = 0;
	}
	for(int pos = 0; pos < 64000; pos++)
	{
		VGA_screen[pos] = 0;
	}

	create_palette_toolbar();

	// When initializing the window, remember to include the palette toolbar when defining the size!
	pixbuf = gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB, false, 8, drawingAreaWidth, (drawingAreaHeight + palette_toolbar_height), (drawingAreaWidth * 3), NULL, NULL);

	GtkWidget *window;
	GtkWidget *grid;
	GtkWidget *button;
	GtkWidget *menu;
	GtkWidget *menu_bar;
	GtkWidget *root_menu;
	GtkWidget *menu_items;
	char buf[128];

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_window_set_title (GTK_WINDOW (window), "Joonas' DOS Game Development Tools - The Image Editor");

	g_signal_connect (window, "destroy", G_CALLBACK (close_window), NULL);

	gtk_container_set_border_width (GTK_CONTAINER (window), 8);

	grid = gtk_grid_new ();

	gtk_container_add (GTK_CONTAINER (window), grid);

	menu = gtk_menu_new();

	root_menu = gtk_menu_item_new_with_label("File");

	gtk_widget_show(root_menu);

	sprintf(buf, "Open");

	menu_items = gtk_menu_item_new_with_label(buf);

	g_signal_connect (menu_items, "activate", G_CALLBACK (menuitemclick), NULL);

	gtk_menu_append(GTK_MENU (menu), menu_items);

	sprintf(buf, "Save As...");

	menu_items = gtk_menu_item_new_with_label(buf);

	g_signal_connect (menu_items, "activate", G_CALLBACK (menuitem2click), NULL);

	gtk_menu_append(GTK_MENU (menu), menu_items);

	gtk_widget_show(menu_items);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM (root_menu), menu);

	menu_bar = gtk_menu_bar_new();
	gtk_grid_attach (GTK_GRID (grid), menu_bar, 0, 0, 1, 1);
	gtk_widget_show(menu_bar);

	gtk_menu_shell_append(GTK_MENU_SHELL (menu_bar), root_menu);

	da = gtk_drawing_area_new ();

	// When initializing the window, remember to include the palette toolbar when defining the size!
	gtk_widget_set_size_request (da, drawingAreaWidth, (drawingAreaHeight + palette_toolbar_height));

	gtk_grid_attach (GTK_GRID (grid), da, 0, 1, 1, 1);

	g_signal_connect (da, "draw",
		G_CALLBACK (draw_cb), NULL);
	g_signal_connect (da,"configure-event",
		G_CALLBACK (configure_event_cb), NULL);
	g_signal_connect (da, "motion-notify-event",
		G_CALLBACK (motion_notify_event_cb), NULL);
	g_signal_connect (da, "button-press-event",
		G_CALLBACK (button_press_event_cb), NULL);

	gtk_widget_set_events (da, gtk_widget_get_events (da)
		| GDK_BUTTON_PRESS_MASK
		| GDK_POINTER_MOTION_MASK);

	slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 63, 1);

	gtk_widget_set_size_request (slider, 100, 25);
	gtk_widget_set_halign(slider, GTK_ALIGN_START);

	gtk_grid_attach (GTK_GRID (grid), slider, 0, 2, 1, 1);

	slider2 = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 63, 1);

	gtk_widget_set_size_request (slider2, 100, 25);
	gtk_widget_set_halign(slider2, GTK_ALIGN_START);

	gtk_grid_attach (GTK_GRID (grid), slider2, 0, 3, 1, 1);

	slider3 = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 63, 1);

	gtk_widget_set_size_request (slider3, 100, 25);
	gtk_widget_set_halign(slider3, GTK_ALIGN_START);

	g_signal_connect (slider, "value-changed",
		G_CALLBACK (slider_R_changed), NULL);
	g_signal_connect (slider2, "value-changed",
		G_CALLBACK (slider_G_changed), NULL);
	g_signal_connect (slider3, "value-changed",
		G_CALLBACK (slider_B_changed), NULL);

	gtk_grid_attach (GTK_GRID (grid), slider3, 0, 4, 1, 1);

	// Text entry field for width
	widthField = gtk_entry_new ();
	gtk_entry_set_width_chars(GTK_ENTRY (widthField), 5);
	gtk_widget_set_halign(widthField, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), widthField, 0, 5, 1, 1);
	g_signal_connect (widthField, "activate",
		G_CALLBACK (widthField_changed), NULL);

	// Text entry field for height
	heightField = gtk_entry_new ();
	gtk_entry_set_width_chars(GTK_ENTRY (heightField), 5);
	gtk_widget_set_halign(heightField, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), heightField, 0, 6, 1, 1);
	g_signal_connect (heightField, "activate",
		G_CALLBACK (heightField_changed), NULL);

	// Text entry field for source color (changing pixels of a certain value to another value)
	sourceColorField = gtk_entry_new ();
	gtk_entry_set_width_chars(GTK_ENTRY (sourceColorField), 5);
	gtk_widget_set_halign(sourceColorField, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), sourceColorField, 0, 7, 1, 1);
	g_signal_connect (sourceColorField, "activate",
		G_CALLBACK (sourceColorField_changed), NULL);

	// Text entry field for target color (changing pixels of a certain value to another value)
	targetColorField = gtk_entry_new ();
	gtk_entry_set_width_chars(GTK_ENTRY (targetColorField), 5);
	gtk_widget_set_halign(targetColorField, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), targetColorField, 0, 8, 1, 1);
	g_signal_connect (targetColorField, "activate",
		G_CALLBACK (targetColorField_changed), NULL);

	gtk_widget_show_all (window);

	int colorR = VGA_palette_registers[(brush1_color * 3) + 0];
	int colorG = VGA_palette_registers[(brush1_color * 3) + 1];
	int colorB = VGA_palette_registers[(brush1_color * 3) + 2];
	gtk_range_set_value(GTK_RANGE (slider), (colorR / 4));
	gtk_range_set_value(GTK_RANGE (slider2), (colorG / 4));
	gtk_range_set_value(GTK_RANGE (slider3), (colorB / 4));

	gtk_main ();

	return 0;
}
