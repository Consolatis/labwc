#ifndef XBM_H
#define XBM_H

enum token_type {
	TOKEN_NONE = 0,
	TOKEN_IDENT,
	TOKEN_INT,
	TOKEN_SPECIAL,
	TOKEN_OTHER,
};

#define MAX_TOKEN_SIZE (256)
struct token {
	char name[MAX_TOKEN_SIZE];
	size_t pos;
	enum token_type type;
};

/**
 * xbm_create_bitmap - parse xbm tokens and create pixmap
 * @tokens: token vector
 */
void xbm_create_bitmap(struct token *tokens);

/**
 * tokenize - tokenize xbm file
 * @buffer: buffer containing xbm file
 * return token vector
 */
struct token *xbm_tokenize(char *buffer);

/**
 * xbm_read_file - read file into buffer (as it's easier to tokenize that way)
 * @filename: file to be read
 * return allocated memory
 */
char *xbm_read_file(const char *filename);

#endif /* XBM_H */
