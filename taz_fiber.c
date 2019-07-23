#include "taz_fiber.h"
#include "taz_engine.h"
#include "taz_code.h"

typedef struct BaseAR BaseAR;
typedef struct ByteAR ByteAR;
typedef struct HostAR HostAR;
typedef enum   ARType ARType;
typedef union  AR     AR;
typedef struct Regs   Regs;

enum ARType {
    BYTE_AR,
    HOST_AR
};

struct BaseAR {
    ARType     type;
    BaseAR*    prev;
    tazR_Fun*  fun;
    tazR_TVal* sb;
};

struct ByteAR {
    BaseAR     base;
    ulongest*  wp;
    unsigned   ws;
};

struct HostAR {
    BaseAR      base;
    taz_LocInfo loc;
    long        cp;
    char        state[];
};

struct tazR_Fib {
    struct {
        tazR_TVal* buf;
        tazR_TVal* top;
        unsigned   cap;
    } vstack;

    struct {
        void*   buf;
        BaseAR* top;
        size_t  cap;
    } cstack;

    tazR_Fib*  parent;
    tazR_TVal  errval;
    taz_ErrNum errnum;
    taz_Trace* trace;
};