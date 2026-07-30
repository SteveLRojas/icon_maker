#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "argparse.h"

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
}
