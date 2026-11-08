#ifndef API_H
#define API_H

char *api_input_buffer(void);
char *api_output_buffer(void);
void  api_init(void);
int   api_solve(int max_nodes, int target_len);
int   api_apply_moves(void);
int   api_last_nodes(void);

#endif
