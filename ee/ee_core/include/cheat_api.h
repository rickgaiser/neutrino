#ifndef _CHEATAPI_H_
#define _CHEATAPI_H_

void EnableCheats(void);
void DisableCheats(void);

/**
 * code_t - a code object
 * @addr: code address
 * @val: code value
 */
typedef struct
{
    u32 addr;
    u32 val;
} code_t;

#endif /* _CHEATAPI_H_ */
