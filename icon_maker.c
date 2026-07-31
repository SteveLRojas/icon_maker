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
	unsigned int offset;
} map_node;

typedef struct MAP_HEAP
{
	map_node* elements;
	unsigned int size;
} map_heap;

typedef struct PIXEL_IMAGE
{
	unsigned int width;
	unsigned int height;
	unsigned int size;
	uint8_t* pixels_red;
	uint8_t* pixels_green;
	uint8_t* pixels_blue;
} pixel_image;

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

void scale_pixel_image(pixel_image* input_image, pixel_image* output_image, unsigned int new_height, unsigned int new_width)
{
	output_image->height = new_height;
	output_image->width = new_width;
	output_image->size = new_height * new_width;
	output_image->pixels_red = (uint8_t*)malloc(sizeof(uint8_t) * output_image->size);
	output_image->pixels_green = (uint8_t*)malloc(sizeof(uint8_t) * output_image->size);
	output_image->pixels_blue = (uint8_t*)malloc(sizeof(uint8_t) * output_image->size);
	
	//scale image
	resize_bilinear(input_image->pixels_red, input_image->width, input_image->height, output_image->pixels_red, new_width, new_height);
	resize_bilinear(input_image->pixels_green, input_image->width, input_image->height, output_image->pixels_green, new_width, new_height);
	resize_bilinear(input_image->pixels_blue, input_image->width, input_image->height, output_image->pixels_blue, new_width, new_height);
}

void create_map_nodes(map_node* output_elements, pixel_image* input_image)
{
	unsigned int element_offset = 0;
	unsigned int rms_error;
	unsigned int min_rms_error;
	int diff;
	unsigned int diff_square;
	
	for(unsigned int node_idx = 0; node_idx < input_image->size; ++node_idx)
	{
		min_rms_error = (unsigned int)(-1);
		for(unsigned int palette_index = 0; palette_index < NUM_COLORS; ++palette_index)
		{
			//compute total diff_square
			//red diff
			diff = (int)(input_image->pixels_red[node_idx]) - (int)(palette_rgb[palette_index * 3]);
			diff_square = (unsigned int)(diff * diff);
			//green diff
			diff = (int)(input_image->pixels_green[node_idx]) - (int)(palette_rgb[palette_index * 3 + 1]);
			diff_square += (unsigned int)(diff * diff);
			//blue diff
			diff = (int)(input_image->pixels_blue[node_idx]) - (int)(palette_rgb[palette_index * 3 + 2]);
			diff_square += (unsigned int)(diff * diff);
			rms_error = (unsigned int)(sqrt((double)diff_square) + 0.5);
			output_elements[element_offset].rms_error[palette_index] = rms_error;
			if(rms_error < min_rms_error)
			{
				min_rms_error = rms_error;
			}
		}
		output_elements[element_offset].min_rms_error = min_rms_error;
		output_elements[element_offset].offset = node_idx;
		++element_offset;
	}
}

void create_cg3_output(uint8_t* cg3_output, map_node* input_elements, unsigned int num_elements)
{
	unsigned int rms_error;
	unsigned int min_rms_error;
	unsigned int error_offset[NUM_COLORS];
	uint8_t best_match = 0;

	for(unsigned int d = 0; d < NUM_COLORS; ++d)
	{
		error_offset[d] = 0;
	}

	for(unsigned int element_offset = 0; element_offset < num_elements; ++element_offset)
	{
		min_rms_error = (unsigned int)(-1);
		for(uint8_t palette_index = 0; palette_index < NUM_COLORS; ++palette_index)
		{	
			rms_error = input_elements[element_offset].rms_error[palette_index];
			rms_error = rms_error + (error_offset[palette_index] >> 10);
			if(rms_error < min_rms_error)
			{
				min_rms_error = rms_error;
				best_match = palette_index;
			}
		}
		cg3_output[input_elements[element_offset].offset] = best_match;
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
		heap.size = end;
		map_heapify_down(&heap, 0);
	}
	return;
}

void color_map_to_rgba(unsigned char* output_image, uint8_t* input_image, unsigned int num_pixels)
{
	uint8_t palette_index;
	unsigned int rgba_offset = 0;
	
	for(unsigned int pixel_idx = 0; pixel_idx < num_pixels; ++pixel_idx)
	{
		palette_index = input_image[pixel_idx];
		output_image[rgba_offset] = palette_rgb[palette_index * 3];
		output_image[rgba_offset + 1] = palette_rgb[palette_index * 3 + 1];
		output_image[rgba_offset + 2] = palette_rgb[palette_index * 3 + 2];
		output_image[rgba_offset + 3] = 255;
		rgba_offset = rgba_offset + 4;
	}
}

void split_image(uint8_t* input, unsigned int num_pixels, uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha)
{
	if(alpha)
	{
		for(unsigned int d = 0; d < num_pixels; ++d)
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
		for(unsigned int d = 0; d < num_pixels; ++d)
		{
			unsigned int pixel = d << 2;
			red[d] = input[pixel];
			green[d] = input[pixel + 1];
			blue[d] = input[pixel + 2];
		}
	}
	return;
}

void merge_image(uint8_t* output, unsigned int num_pixels, uint8_t* red, uint8_t* green, uint8_t* blue, uint8_t* alpha)
{
	if(alpha)
	{
		for(unsigned int d = 0; d < num_pixels; ++d)
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
		for(unsigned int d = 0; d < num_pixels; ++d)
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

//TODO: Add option to upscale final output by an integer factor
//TODO: Add option to read custom color palette, per color initial cost, and per color reuse cost
int main(int argc, char** argv)
{
	parse_args(argc, argv);
	
	if(cost_index)
	{
		color_cost = (uint8_t)atol(argv[cost_index]);
		printf("Color cost factor is: %u\n", color_cost);
	}

	unsigned char* image;
	pixel_image input_image;
	
	unsigned error = lodepng_decode32_file(&image, &(input_image.width), &(input_image.height), argv[source_index]);
	if(error)
	{
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}
	printf("Loaded image\n");
	printf("Width is: %u\n", input_image.width);
	printf("Height is: %u\n", input_image.height);
	if(debug_enable)
	{
		printf("Image red channel at 0: %u\n", image[0]);
		printf("Image green channel at 0: %u\n", image[1]);
		printf("Image blue channel at 0: %u\n", image[2]);
		printf("Image alpha channel at 0: %d\n", image[3]);
	}

	input_image.size = input_image.width * input_image.height;
	input_image.pixels_red = (uint8_t*)malloc(sizeof(uint8_t) * input_image.size);
	input_image.pixels_green = (uint8_t*)malloc(sizeof(uint8_t) * input_image.size);
	input_image.pixels_blue = (uint8_t*)malloc(sizeof(uint8_t) * input_image.size);

	split_image((uint8_t*)image, input_image.size, input_image.pixels_red, input_image.pixels_green, input_image.pixels_blue, NULL);
	free(image);
	printf("Split input image\n");
	
	//Create scaled image
	pixel_image scaled_image;
	scale_pixel_image(&input_image, &scaled_image, 192, 256);
	free(input_image.pixels_blue);
	free(input_image.pixels_green);
	free(input_image.pixels_red);
	printf("Scaled input image\n");

	//create cg3 elements
	map_node cg3_display_elements[scaled_image.size];
	create_map_nodes(cg3_display_elements, &scaled_image);
	printf("Created color map nodes\n");
	map_heapsort(cg3_display_elements, scaled_image.size);
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
	if(scaled_index)
	{
		//convert RGB image to RGBA image
		unsigned char* output_image = (unsigned char*)malloc(4 * scaled_image.size * sizeof(unsigned char));
		merge_image(output_image, scaled_image.size, scaled_image.pixels_red, scaled_image.pixels_green, scaled_image.pixels_blue, NULL);
		printf("Merged scaled image\n");

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
	
	free(scaled_image.pixels_blue);
	free(scaled_image.pixels_green);
	free(scaled_image.pixels_red);
	
	//create cg3 image
	uint8_t cg3_image[scaled_image.size];
	create_cg3_output(cg3_image, cg3_display_elements, scaled_image.size);
	printf("Created CG3 image\n");

	//create cg3 preview
	unsigned char* mapped_image = (unsigned char*)malloc(4 * scaled_image.size * sizeof(unsigned char));
	color_map_to_rgba(mapped_image, cg3_image, scaled_image.size);
	printf("Created color-mapped image\n");
	//TODO: enforce PNG file extension
	error = lodepng_encode32_file(argv[out_index], mapped_image, scaled_image.width, scaled_image.height);
	if(error)
	{
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}
	free(mapped_image);
	printf("Wrote output image\n");

	return 0;
}
