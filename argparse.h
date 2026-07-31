#ifndef _ARGPARSE_H_
#define _ARGPARSE_H_

extern uint8_t source_index;
extern uint8_t out_index;
extern uint8_t scaled_index;
extern uint8_t cost_index;
extern uint8_t shift_index;
extern uint8_t palette_index;
extern uint8_t debug_enable;
extern uint8_t reuse_cost;
extern uint8_t reuse_shift;
extern unsigned int palette_size;

int str_comp_partial(const char* str1, const char* str2);
void to_caps(char* str);
void replace_file_extension(char* new_ext, char* out_name, char* new_name);
void parse_args(int argc, char** argv);
uint8_t* load_palette(const char* filename);

#endif
