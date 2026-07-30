#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "lodepng.h"
#include "argparse.h"

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define NUM_COLORS 8

uint8_t palette_rgb[] =
{
	0x00, 0xff, 0x00,	// GREEN
	0xff, 0xff, 0x00,	// YELLOW
	0x00, 0x00, 0xff,	// BLUE
	0xff, 0x00, 0x00,	// RED
	0xff, 0xff, 0xff,	// BUFF
	0x00, 0xff, 0xff,	// CYAN
	0xff, 0x00, 0xff,	// MAGENTA
	0xff, 0x80, 0x00,	// ORANGE
};

typedef struct MAP_NODE
{
	unsigned int rms_error[NUM_COLORS];
	unsigned int min_rms_error;
	unsigned int source_offset_y;
	unsigned int source_offset_x;
} map_node;

typedef struct MAP_HEAP
{
	map_node* elements;
	unsigned int size;
} map_heap;

typedef struct PIXEL_IMAGE
{
	uint8_t** pixels_red;	//TODO: change these to single pointers?
	uint8_t** pixels_green;
	uint8_t** pixels_blue;
	unsigned int width;
	unsigned int height;
} pixel_image;

void create_pixel_image(pixel_image* input_image, unsigned int height, unsigned int width)
{
	input_image->pixels_red = (uint8_t**)malloc(height * sizeof(uint8_t*));
	input_image->pixels_green = (uint8_t**)malloc(height * sizeof(uint8_t*));
	input_image->pixels_blue = (uint8_t**)malloc(height * sizeof(uint8_t*));
	for(unsigned int y = 0; y < height; ++y)
	{
		input_image->pixels_red[y] = (uint8_t*)malloc(width * sizeof(uint8_t));
		input_image->pixels_green[y] = (uint8_t*)malloc(width * sizeof(uint8_t));
		input_image->pixels_blue[y] = (uint8_t*)malloc(width * sizeof(uint8_t));
		for(unsigned int x = 0; x < width; ++x)
		{
			input_image->pixels_red[y][x] = 0;
			input_image->pixels_green[y][x] = 0;
			input_image->pixels_blue[y][x] = 0;
		}
	}
	input_image->width = width;
	input_image->height = height;
	return;
}

void delete_pixel_image(pixel_image* input_image)
{
	for(unsigned int y = 0; y < input_image->height; ++y)
	{
		free(input_image->pixels_red[y]);
		free(input_image->pixels_green[y]);
		free(input_image->pixels_blue[y]);
	}
	free(input_image->pixels_red);
	free(input_image->pixels_green);
	free(input_image->pixels_blue);
	return;
}

void rgb_to_pixel_image(pixel_image output, uint8_t* input_red, uint8_t* input_green, uint8_t* input_blue)
{
	unsigned int in_index;
	for(unsigned int y = 0; y < output.height; ++y)
	{
		for(unsigned int x = 0; x < output.width; ++x)
		{
			in_index = (output.width * y) + x;
			output.pixels_red[y][x] = input_red[in_index];
			output.pixels_green[y][x] = input_green[in_index];
			output.pixels_blue[y][x] = input_blue[in_index];
		}
	}
	return;
}

void fill_RGBA_image(unsigned char* output_image, pixel_image* input_image)	//TODO: check if this can be optimized
{
	unsigned int image_size = 4 * input_image->height * input_image->width;
	unsigned int x;
	unsigned int y;
	for(unsigned int d = 0; d < image_size; d = d + 4)
	{
		x = (d >> 2) % input_image->width;
		y = (d >> 2) / input_image->width;
		output_image[d] = input_image->pixels_red[y][x];
		output_image[d + 1] = input_image->pixels_green[y][x];
		output_image[d + 2] = input_image->pixels_blue[y][x];
		output_image[d + 3] = 255;
	}
}

void create_map_nodes(map_node* output_elements, pixel_image* input_image)
{
	unsigned int element_offset = 0;
	unsigned int rms_error;
	unsigned int min_rms_error;
	int diff;
	unsigned int diff_square;
	for(unsigned int y = 0; y < input_image->height; ++y)
	{
		for(unsigned int x = 0; x < input_image->width; ++x)
		{
			min_rms_error = (unsigned int)(-1);
			for(unsigned int palette_index = 0; palette_index < NUM_COLORS; ++palette_index)
			{
				//compute total diff_square
				//red diff
				diff = (int)(input_image->pixels_red[y][x]) - (int)(palette_rgb[palette_index * 3]);
				diff_square = (unsigned int)(diff * diff);
				//green diff
				diff = (int)(input_image->pixels_green[y][x]) - (int)(palette_rgb[palette_index * 3 + 1]);
				diff_square += (unsigned int)(diff * diff);
				//blue diff
				diff = (int)(input_image->pixels_blue[y][x]) - (int)(palette_rgb[palette_index * 3 + 2]);
				diff_square += (unsigned int)(diff * diff);
				rms_error = (unsigned int)(sqrt((double)diff_square) + 0.5);
				output_elements[element_offset].rms_error[palette_index] = rms_error;
				if(rms_error < min_rms_error)
				{
					min_rms_error = rms_error;
				}
			}
			output_elements[element_offset].min_rms_error = min_rms_error;
			output_elements[element_offset].source_offset_y = y;
			output_elements[element_offset].source_offset_x = x;
			++element_offset;
		}
	}
}

//TODO: store element offset instead of source X and Y location
void create_cg3_output(uint8_t* cg3_output, map_node* input_elements, unsigned int height, unsigned int width)
{
	unsigned int rms_error;
	unsigned int min_rms_error;
	unsigned int error_offset[NUM_COLORS];
	unsigned int x;
	unsigned int y;
	uint8_t best_match = 0;
	unsigned int output_offset;

	for(unsigned int d = 0; d < NUM_COLORS; ++d)
	{
		error_offset[d] = 0;
	}

	for(unsigned int element_offset = 0; element_offset < (height * width); ++element_offset)
	{
		min_rms_error = (unsigned int)(-1);
		x = input_elements[element_offset].source_offset_x;
		y = input_elements[element_offset].source_offset_y;
		for(uint8_t palette_index = 0; palette_index < NUM_COLORS; ++palette_index)
		{	
			rms_error = input_elements[element_offset].rms_error[palette_index];
			rms_error = rms_error + (error_offset[palette_index] >> 8);
			if(rms_error < min_rms_error)
			{
				min_rms_error = rms_error;
				best_match = palette_index;
			}
		}
		output_offset = y * width + x;	//recover element index
		cg3_output[output_offset] = best_match;
		error_offset[best_match] = error_offset[best_match] + color_cost;	//tweak the increment for best results
		//low increments are better for images with very uniform color
	}
}

void map_heapify_down(map_heap* heap, unsigned int index)
{
	unsigned int left_index;
	unsigned int right_index;
	unsigned int largest;
	map_node temp;
	while(1)
	{
		left_index = index * 2 + 1;
		right_index = index * 2 + 2;
		largest = index;
		if((left_index < heap->size) && (heap->elements[left_index].min_rms_error > heap->elements[largest].min_rms_error))
			largest = left_index;
		if((right_index < heap->size) && (heap->elements[right_index].min_rms_error > heap->elements[largest].min_rms_error))
			largest = right_index;
		if(largest == index)
			return;
		temp = heap->elements[index];
		heap->elements[index] = heap->elements[largest];
		heap->elements[largest] = temp;
		index = largest;
	}
}

void map_heapify(map_heap* heap, map_node* elements, unsigned int size)
{
	heap->size = size;
	heap->elements = elements;
	for(unsigned int index = size - 1; index != (unsigned int)(-1); --index)
	{
		map_heapify_down(heap, index);
	}
	return;
}

void map_heapsort(map_node* elements, unsigned int size)
{
	map_node temp;
	map_heap heap;
	map_heapify(&heap, elements, size);
	for(unsigned int end = size - 1; end != (unsigned int)(-1); --end)
	{
		temp = elements[end];
		elements[end] = elements[0];
		elements[0] = temp;
		--size;
		map_heapify(&heap, elements, size);
	}
	return;
}

void cg3_to_rgba(unsigned char* output_image, uint8_t* input_image, unsigned int height, unsigned int width)
{
	unsigned int cg3_offset;
	//uint8_t pair_offset;
	uint8_t palette_index;
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	unsigned int rgba_offset = 0;
	for(unsigned int y = 0; y < height; ++y)
	{
		for(unsigned int x = 0; x < width; ++x)
		{
			cg3_offset = y * width + x;	//determine element offset
			palette_index = input_image[cg3_offset];
			//get colors
			red = palette_rgb[palette_index * 3];
			green = palette_rgb[palette_index * 3 + 1];
			blue = palette_rgb[palette_index * 3 + 2];
			output_image[rgba_offset] = red;
			output_image[rgba_offset + 1] = green;
			output_image[rgba_offset + 2] = blue;
			output_image[rgba_offset + 3] = 255;
			rgba_offset = rgba_offset + 4;
		}
	}
}

void split_image(uint8_t* input, unsigned int height, unsigned int width, uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha)
{
	if(alpha)
	{
		for(unsigned int d = 0; d < height * width; ++d)
		{
			unsigned int pixel = d << 2;
			red[d] = input[pixel];
			green[d] = input[pixel + 1];
			blue[d] = input[pixel + 2];
			alpha[d] = input[pixel + 3];
		}
	}
	else
	{
		for(unsigned int d = 0; d < height * width; ++d)
		{
			unsigned int pixel = d << 2;
			red[d] = input[pixel];
			green[d] = input[pixel + 1];
			blue[d] = input[pixel + 2];
		}
	}
	return;
}
/*
void merge_image(uint8_t* output, unsigned int height, unsigned int width, uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha)
{
	if(alpha)
	{
		for(unsigned int d = 0; d < height * width; ++d)
		{
			unsigned int pixel = d << 2;
			output[pixel + 0] = red[d];
			output[pixel + 1] = green[d];
			output[pixel + 2] = blue[d];
			output[pixel + 3] = alpha[d];
		}
	}
	else
	{
		for(unsigned int d = 0; d < height * width; ++d)
		{
			unsigned int pixel = d << 2;
			output[pixel + 0] = red[d];
			output[pixel + 1] = green[d];
			output[pixel + 2] = blue[d];
			output[pixel + 3] = 255;
		}
	}
	return;
}
*/
	//TODO: check if this can be optimized (maybe use integer arithmetic?)
void resize_bilinear(const uint8_t *src, unsigned int src_width, unsigned int src_height, uint8_t *dst, unsigned int dst_width, unsigned int dst_height)
{
    float scale_x;
    float scale_y;

    if(dst_width > 1)
    {
        scale_x = (float)(src_width - 1) / (float)(dst_width - 1);
    }
    else
    {
        scale_x = 0.0f;
    }

    if(dst_height > 1)
    {
        scale_y = (float)(src_height - 1) / (float)(dst_height - 1);
    }
    else
    {
        scale_y = 0.0f;
    }

    for(unsigned int y = 0; y < dst_height; ++y)
    {
        float ys = (float)y * scale_y;

        unsigned int y0 = (unsigned int)ys;
        unsigned int y1 = y0 + 1;

        if(y1 >= src_height)
            y1 = src_height - 1;

        float fy = ys - (float)y0;

        for(unsigned int x = 0; x < dst_width; ++x)
        {
            float xs = (float)x * scale_x;

            unsigned int x0 = (unsigned int)xs;
            unsigned int x1 = x0 + 1;

            if(x1 >= src_width)
                x1 = src_width - 1;

            float fx = xs - (float)x0;

            uint8_t p00 = src[y0 * src_width + x0];
            uint8_t p10 = src[y0 * src_width + x1];
            uint8_t p01 = src[y1 * src_width + x0];
            uint8_t p11 = src[y1 * src_width + x1];

            float value = (1.0f - fx) * (1.0f - fy) * (float)p00 + fx * (1.0f - fy) * (float)p10 + (1.0f - fx) * fy * (float)p01 + fx * fy * (float)p11;

            dst[y * dst_width + x] = (uint8_t)(value + 0.5f);
        }
    }
}

//TODO: Add option to upscale final output by an integer factor
//TODO: Add option to read custom color palette, per color initial cost, and per color reuse cost
//TODO: sorting map nodes is taking too long, optimize.
int main(int argc, char** argv)
{
	parse_args(argc, argv);
	
	unsigned char* image;
	unsigned int width, height;

	if(cost_index)
	{
		color_cost = (uint8_t)atol(argv[cost_index]);
		printf("Color cost factor is: %u\n", color_cost);
	}

	unsigned error = lodepng_decode32_file(&image, &width, &height, argv[source_index]);
	if(error)
	{
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}
	printf("Loaded image\n");
	printf("Width is: %u\n", width);
	printf("Height is: %u\n", height);
	if(debug_enable)
	{
		printf("Image red channel at 0: %u\n", image[0]);
		printf("Image green channel at 0: %u\n", image[1]);
		printf("Image blue channel at 0: %u\n", image[2]);
		printf("Image alpha channel at 0: %d\n", image[3]);
	}

	uint8_t* input_red;
	uint8_t* input_green;
	uint8_t* input_blue;

	input_red = (uint8_t*)malloc(sizeof(uint8_t) * width * height);
	input_green = (uint8_t*)malloc(sizeof(uint8_t) * width * height);
	input_blue = (uint8_t*)malloc(sizeof(uint8_t) * width * height);

	split_image((uint8_t*)image, height, width, input_red, input_green, input_blue, NULL);
	free(image);
	
	//allocate scaled image buffers
	unsigned int new_height = 192;
	unsigned int new_width = 256;
	
	uint8_t* scaled_red;
	uint8_t* scaled_green;
	uint8_t* scaled_blue;
	
	scaled_red = (uint8_t*)malloc(sizeof(uint8_t) * new_width * new_height);
	scaled_green = (uint8_t*)malloc(sizeof(uint8_t) * new_width * new_height);
	scaled_blue = (uint8_t*)malloc(sizeof(uint8_t) * new_width * new_height);
	printf("Created scaled image\n");
	
	//scale image
	resize_bilinear(input_red, width, height, scaled_red, new_width, new_height);
	resize_bilinear(input_green, width, height, scaled_green, new_width, new_height);
	resize_bilinear(input_blue, width, height, scaled_blue, new_width, new_height);
	free(input_red);
	free(input_green);
	free(input_blue);
	printf("Resized input image\n");
	
	//create scaled RGB image
	pixel_image scaled_image;
	create_pixel_image(&scaled_image, new_height, new_width);
	printf("Created new RGB image\n");

	rgb_to_pixel_image(scaled_image, scaled_red, scaled_green, scaled_blue);
	free(scaled_red);
	free(scaled_green);
	free(scaled_blue);
	printf("Filled in new RGB image\n");

	//create cg3 elements
	map_node cg3_display_elements[new_width * new_height];
	create_map_nodes(cg3_display_elements, &scaled_image);
	printf("Created color map nodes\n");
	map_heapsort(cg3_display_elements, new_width * new_height);
	printf("Sorted color map nodes\n");

	if(debug_enable)
	{
		printf("Top 25 RMS errors:\n");
		for(unsigned int d = 0; d < 25; ++d)
		{
			printf("%u\n", cg3_display_elements[d].min_rms_error);
		}
	}
	
	//Output scaled image
	unsigned int image_size;
	if(scaled_index)
	{
		//convert RGB image to RGBA image
		image_size = 4 * scaled_image.height * scaled_image.width;
		unsigned char* output_image = (unsigned char*)malloc(image_size * sizeof(unsigned char));
		fill_RGBA_image(output_image, &scaled_image);
		printf("Converted RGB image to RGBA\n");

		//Write scaled image to file
		//TODO: enforce PNG file extension
		error = lodepng_encode32_file(argv[scaled_index], output_image, scaled_image.width, scaled_image.height);
		if(error)
		{
			printf("error %u: %s\n", error, lodepng_error_text(error));
			return 1;
		}
		free(output_image);
		printf("Wrote scaled image\n");
	}
	
	delete_pixel_image(&scaled_image);
	
	//create cg3 image
	uint8_t cg3_image[new_width * new_height];
	create_cg3_output(cg3_image, cg3_display_elements, new_height, new_width);
	printf("Created CG3 image\n");

	//create cg3 preview
	image_size = 4 * 192 * 256;
	unsigned char* rgba_cg3_preview = (unsigned char*)malloc(image_size * sizeof(unsigned char));
	cg3_to_rgba(rgba_cg3_preview, cg3_image, new_height, new_width);
	printf("Created CG3 preview\n");
	//TODO: enforce PNG file extension
	error = lodepng_encode32_file(argv[out_index], rgba_cg3_preview, new_width, new_height);
	if(error)
	{
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}
	free(rgba_cg3_preview);
	printf("Wrote CG3 preview\n");

	return 0;
}
