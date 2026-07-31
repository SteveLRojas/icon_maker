#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "argparse.h"

const char png_string[] = ".png";
const char source_string[] = "-SOURCE";
const char out_string[] = "-OUT";
const char scaled_string[] = "-SCALED";
const char cost_string[] = "-COST";
const char shift_string[] = "-SHIFT";
const char palette_string[] = "-PALETTE";
const char width_string[] = "-WIDTH";
const char height_string[] = "-HEIGHT";
const char debug_string[] = "-DEBUG";

uint8_t source_index = 0;
uint8_t out_index = 0;
uint8_t scaled_index = 0;
uint8_t cost_index = 0;
uint8_t shift_index = 0;
uint8_t palette_index = 0;
uint8_t width_index = 0;
uint8_t height_index = 0;

uint8_t debug_enable = 0;
uint8_t reuse_cost = 0;
uint8_t reuse_shift = 10;
unsigned int palette_size = 0;
uint8_t* palette_rgb = NULL;
uint16_t new_width = 256;
uint16_t new_height = 192;

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

void parse_args(int argc, char** argv)
{
	//Parse program arguments
	unsigned int arg = 1;
	if(argc == 1)
	{
		printf("Usage: -SOURCE <source file> -OUT <output image> -SCLAED <output scaled image>\n\t-COST <color cost factor> -SHIFT <cost shift factor> -PALETTE <palette file>\n");
		printf("\t-WIDHT <new width> -HEIGHT <new height> -DEBUG\n");
		printf("-SCALED, -COST, -PALETTE, -WIDTH, -HEIGHT, and -DEBUG are optional\n");
		printf("The color cost factor is 0 by default, max is 255. Increase this value to reduce color reuse.\n");
		printf("The color cost shift factor is 10 by default, max is 31. Higher values weaken the effect of the color cost factor.\n");
		printf("The default palette file is cg3.txt. Max palette size is %u.\n", MAX_PALETTE_SIZE);
		printf("Default width is 256, default height is 192.\n");
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
			else if(str_comp_partial(shift_string, argv[arg]))
			{
				shift_index = (uint8_t)++arg;
			}
			else if(str_comp_partial(palette_string, argv[arg]))
			{
				palette_index = (uint8_t)++arg;
			}
			else if(str_comp_partial(width_string, argv[arg]))
			{
				width_index = (uint8_t)++arg;
			}
			else if(str_comp_partial(height_string, argv[arg]))
			{
				height_index = (uint8_t)++arg;
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
	
	if(cost_index)
	{
		reuse_cost = (uint8_t)atol(argv[cost_index]);
		printf("Color reuse cost factor is: %u\n", reuse_cost);
	}
	if(shift_index)
	{
		reuse_shift = (uint8_t)atol(argv[shift_index]);
		printf("Color reuse shift factor is: %u\n", reuse_shift);
	}
	
	if(palette_index)
	{
		palette_rgb = load_palette(argv[palette_index]);
	}
	else
	{
		palette_rgb = load_palette("cg3.txt");
	}
	if(palette_size > MAX_PALETTE_SIZE)
	{
		printf("Maximum palette size exceeded! Truncating to %u colors.\n", MAX_PALETTE_SIZE);
		palette_size = MAX_PALETTE_SIZE;
	}
	
	if(width_index)
	{
		new_width = (uint16_t)atol(argv[width_index]);
		printf("New width is %u\n", new_width);
	}
	if(height_index)
	{
		new_height = (uint16_t)atol(argv[height_index]);
		printf("New height is %u\n", new_height);
	}
}

uint8_t* load_palette(const char* filename)
{
    FILE* fp;
    char line[128];
    unsigned int color_count = 0;
    uint8_t* palette;
    unsigned int rgb;

    fp = fopen(filename, "r");

    if(fp == NULL)
    {
        fprintf(stderr, "Failed to open palette file: %s\n", filename);
        return NULL;
    }

    /* First pass: count colors */
    while(fgets(line, sizeof(line), fp))
    {
        char* ptr = line;

        while((*ptr == ' ') || (*ptr == '\t') || (*ptr == '#'))
        {
            ++ptr;
        }

        if((*ptr == '\0') || (*ptr == '\n') || (*ptr == ';'))
        {
            continue;
        }

        ++color_count;
    }

    if(color_count == 0)
    {
        fclose(fp);
        fprintf(stderr, "Palette file contains no colors\n");
        return NULL;
    }

    palette = (uint8_t*)malloc(color_count * 3);
    
    /* Second pass: load colors */
	rewind(fp);
    color_count = 0;

    while(fgets(line, sizeof(line), fp))
    {
        char* ptr = line;

        while((*ptr == ' ') || (*ptr == '\t') || (*ptr == '#'))
        {
            ++ptr;
        }

        if((*ptr == '\0') || (*ptr == '\n') || (*ptr == ';'))
        {
            continue;
        }

        if(sscanf(ptr, "%06x", &rgb) != 1)
        {
            fprintf(stderr, "Invalid palette entry: %s", line);
            free(palette);
            fclose(fp);
            return NULL;
        }

        palette[color_count * 3 + 0] = (rgb >> 16) & 0xFF;
        palette[color_count * 3 + 1] = (rgb >> 8) & 0xFF;
        palette[color_count * 3 + 2] = rgb & 0xFF;

        ++color_count;
    }

    fclose(fp);
    palette_size = color_count;
    printf("Loaded %u palette colors\n", palette_size);
    return palette;
}
