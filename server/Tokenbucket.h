#ifndef __TOKENBUCKET_h__
#define __TOKENBUCKET_h__

#define TOKENBUCKET_MAX 1024

typedef void tokenbt_t;

tokenbt_t *tokenbt_init(int, int);

int tokenbt_fetchtoken(tokenbt_t *, int);

int tokenbt_checktoken(tokenbt_t *);

int tokenbt_returntoken(tokenbt_t *, int);

int tokenbt_destroy(tokenbt_t *);

int tokenbt_destroy_all();

#endif // !__TOKENBUCKET_h__