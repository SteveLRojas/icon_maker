#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "lodepng.h"

#define PI 3.14159265358979323846

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

#define NUM_COLORS 8

const char png_string[] = ".png";
const char source_string[] = "-SOURCE";
const char out_string[] = "-OUT";
const char scaled_string[] = "-SCALED";
const char cost_string[] = "-COST";
const char debug_string[] = "-DEBUG";

uint8_t source_index = 0;
uint8_t out_index = 0;
uint8_t scaled_index = 0;
uint8_t cost_index = 0;
uint8_t debug_enable;
uint8_t color_cost = 0;

uint8_t CG3_PALETTE[] =
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

typedef struct CG3_ELEMENT
{
	unsigned int rms_error[NUM_COLORS];
	unsigned int min_rms_error;
	uint8_t source_offset_y;
	uint8_t source_offset_x;
} cg3_element;

typedef struct CG3_HEAP
{
	cg3_element* elements;
	unsigned int size;
} cg3_heap;

typedef struct PIXEL_IMAGE
{
	uint8_t** pixels_red;
	uint8_t** pixels_green;
	uint8_t** pixels_blue;
	unsigned int width;
	unsigned int height;
} pixel_image;

int str_comp_partial(const char* str1, const char* str2)
{
	for(int i = 0; str1[i] && str2[i]; ++i)
	{
		if(str1[i] != str2[i])
			return 0;
	}
	return 1;
}

void to_caps(char* str)
{
	unsigned int d = 0;
	char prev_char = 0x00;
	while(str[d])
	{
		//do not modifiy substrings that appear in quotes
		if(str[d] == 0x22)	//double quotes
		{
			while(1)
			{
				++d;
				if(!str[d])
					return;
				if(str[d] == 0x22 && prev_char != 0x5C)
					break;
				prev_char = str[d];
			}
		}
		if(str[d] == 0x27)	//single quotes
		{
			while(1)
			{
				++d;
				if(!str[d])
					return;
				if(str[d] == 0x27 && prev_char != 0x5C)
					break;
				prev_char = str[d];
			}
		}
		if(str[d] == ';')	//skip comments
			return;
		if((str[d] > 0x60) & (str[d] < 0x7B))
			str[d] = str[d] - 0x20;
		prev_char = str[d];
		++d;
	}
	return;
}

void replace_file_extension(char* new_ext, char* out_name, char* new_name)
{
	uint8_t count = 0;
	uint8_t count_copy;
	//copy string and get its length
	do
	{
		new_name[count] = out_name[count];
		++count;
	}while(out_name[count]);
	count_copy = count;
	//search for a '.' starting from the end
	while(count)
	{
		count = count - 1;
		if(new_name[count] == '.')	//replace it and what follows with the new extension
		{
			for(unsigned int d = 0; d < 5; ++d)
			{
				new_name[count] = new_ext[d];
				count = count + 1;
			}
			break;
		}
	}
	//check if the file name had no '.'
	if(count == 0)
	{
		count = count_copy;
		for(unsigned int d = 0; d < 5; ++d)
		{
			new_name[count] = new_ext[d];
			count = count + 1;
		}
	}
}

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

void create_cg3_elements(cg3_element* output_elements, pixel_image* input_image)
{
	unsigned int element_offset = 0;
	unsigned int rms_error;
	unsigned int min_rms_error;
	unsigned int diff_square;
	for(unsigned int y = 0; y < input_image->height; y = y + 2)
	{
		for(unsigned int x = 0; x < input_image->width; x = x + 2)
		{
			min_rms_error = 0xffffffff;
			for(unsigned int palette_index = 0; palette_index < NUM_COLORS; ++palette_index)
			{
				//compute total diff_square
				//red diff
				diff_square = (unsigned int)pow((double)((int)(input_image->pixels_red[y][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
				//green diff
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y][x]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
				//blue diff
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
				diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
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

void create_cg3_output(uint8_t* cg3_output, cg3_element* input_elements, pixel_image* input_image)
{
	unsigned int rms_error;
	unsigned int min_rms_error;
	unsigned int diff_square;
	unsigned int error_offset[NUM_COLORS];
	unsigned int x;
	unsigned int y;
	uint8_t best_match = 0;
	unsigned int output_offset;

	for(unsigned int d = 0; d < NUM_COLORS; ++d)
	{
		error_offset[d] = 0;
	}

	for(unsigned int element_offset = 0; element_offset < 12288; ++element_offset)
	{
		min_rms_error = (unsigned int)(-1);
		x = input_elements[element_offset].source_offset_x;
		y = input_elements[element_offset].source_offset_y;
		for(uint8_t palette_index = 0; palette_index < NUM_COLORS; ++palette_index)
		{
			//compute total diff_square
			//red diff
			diff_square = (unsigned int)pow((double)((int)(input_image->pixels_red[y][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_red[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3])), 2);
			//green diff
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y][x]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_green[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 1])), 2);
			//blue diff
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
			diff_square += (unsigned int)pow((double)((int)(input_image->pixels_blue[y + 1][x + 1]) - (int)(CG3_PALETTE[palette_index * 3 + 2])), 2);
			rms_error = (unsigned int)(sqrt((double)diff_square) + 0.5);
			rms_error = rms_error + (error_offset[palette_index] >> 8);
			if(rms_error < min_rms_error)
			{
				min_rms_error = rms_error;
				best_match = palette_index;
			}
		}
		output_offset = (y >> 1) * 128 + (x >> 1);	//recover element index
		cg3_output[output_offset] = best_match;
		error_offset[best_match] = error_offset[best_match] + color_cost;	//tweak the increment for best results
		//low increments are better for images with very uniform color
	}
}

void cg3_heapify_down(cg3_heap* heap, unsigned int index)
{
	unsigned int left_index;
	unsigned int right_index;
	unsigned int largest;
	cg3_element temp;
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

void cg3_heapify(cg3_heap* heap, cg3_element* elements, unsigned int size)
{
	heap->size = size;
	heap->elements = elements;
	for(unsigned int index = size - 1; index != (unsigned int)(-1); --index)
	{
		cg3_heapify_down(heap, index);
	}
	return;
}

void cg3_heapsort(cg3_element* elements, unsigned int size)
{
	cg3_element temp;
	cg3_heap heap;
	cg3_heapify(&heap, elements, size);
	for(unsigned int end = size - 1; end != (unsigned int)(-1); --end)
	{
		temp = elements[end];
		elements[end] = elements[0];
		elements[0] = temp;
		--size;
		cg3_heapify(&heap, elements, size);
	}
	return;
}

void cg3_to_rgba(unsigned char* output_image, uint8_t* input_image)
{
	unsigned int cg3_offset;
	//uint8_t pair_offset;
	uint8_t palette_index;
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	unsigned int rgba_offset = 0;
	for(unsigned int y = 0; y < 192; ++y)
	{
		for(unsigned int x = 0; x < 256; ++x)
		{
			cg3_offset = (y >> 1) * 128 + (x >> 1);	//determine element offset
			palette_index = input_image[cg3_offset];
			//get colors
			red = CG3_PALETTE[palette_index * 3];
			green = CG3_PALETTE[palette_index * 3 + 1];
			blue = CG3_PALETTE[palette_index * 3 + 2];
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
	//TODO: reformat and scrutinize this
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

int main(int argc, char** argv)
{
	//Parse program arguments
	debug_enable = 0;
	unsigned int arg = 1;
	if(argc == 1)
	{
		printf("Usage: -SOURCE <source file> -OUT <output image> -SCLAED <output scaled image> -COST <color cost factor> -DEBUG\n");
		printf("-SCALED, -COST, and -DEBUG are optional\n");
		printf("The color cost factor is 0 by default, max is 255. Increase this value to reduce color reuse.\n");
		exit(1);
	}
	while(arg < (unsigned int)argc)
	{
		if(argv[arg][0] == '-')
		{
			to_caps(argv[arg]);
			if(str_comp_partial(source_string, argv[arg]))
			{
				source_index = (uint8_t)++arg;
			}
			else if(str_comp_partial(out_string, argv[arg]))
			{
				out_index = (uint8_t)++arg;
			}
			else if(str_comp_partial(scaled_string, argv[arg]))
			{
				scaled_index = (uint8_t)++arg;
			}
			else if(str_comp_partial(cost_string, argv[arg]))
			{
				cost_index = (uint8_t)++arg;
			}
			else if(str_comp_partial(debug_string, argv[arg]))
			{
				debug_enable = 0xFF;
			}
			++arg;
		}
		else
		{
			source_index = (uint8_t)arg++;
		}
	}
	if(arg > (unsigned int)argc)
	{
		printf("Invalid arguments!\n");
		exit(1);
	}
	if(source_index == 0)
	{
		printf("No source file specified!\n");
		exit(1);
	}
	if(out_index == 0)
	{
		printf("No output file specified!\n");
		exit(1);
	}

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
	cg3_element cg3_display_elements[12288];
	create_cg3_elements(cg3_display_elements, &scaled_image);
	cg3_heapsort(cg3_display_elements, 12288);
	printf("Created and sorted display elements\n");

	if(debug_enable)
	{
		printf("Top 25 RMS errors:\n");
		for(unsigned int d = 0; d < 25; ++d)
		{
			printf("%u\n", cg3_display_elements[d].min_rms_error);
		}
	}
	
	//create cg3 image
	uint8_t cg3_image[12288];
	create_cg3_output(cg3_image, cg3_display_elements, &scaled_image);
	printf("Created CG3 image\n");

	//create cg3 preview
	unsigned int image_size;
	image_size = 4 * 192 * 256;
	unsigned char* rgba_cg3_preview = (unsigned char*)malloc(image_size * sizeof(unsigned char));
	cg3_to_rgba(rgba_cg3_preview, cg3_image);
	printf("Created CG3 preview\n");
	//TODO: enforce PNG file extension
	error = lodepng_encode32_file(argv[out_index], rgba_cg3_preview, 256, 192);
	if(error)
	{
		printf("error %u: %s\n", error, lodepng_error_text(error));
		return 1;
	}
	free(rgba_cg3_preview);
	printf("Wrote CG3 preview\n");

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
	return 0;
}
