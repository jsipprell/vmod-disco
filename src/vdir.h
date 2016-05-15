#define _munge(sym) f3j44fddf_disco_##sym

#define vdir_add_backend _munge(vdir_add_backend)
#define vdir_any_healthy _munge(vdir_any_healthy)
#define vdir_delete _munge(vdir_delete)
#define vdir_new _munge(vdir_new)
#define vdir_pick_be _munge(vdir_pick_be)
#define vdir_rdlock _munge(vdir_rdlock)
#define vdir_remove_backend _munge(vdir_remove_backend)
#define vdir_unlock _munge(vdir_unlock)
#define vdir_wrlock _munge(vdir_wrlock)
#define vdir_exact_be _munge(vdir_exact_be)
#define vdir_expand _munge(vdir_expand)
#define vdir_pick_by_weight _munge(vdir_pick_by_weight)
#include "_vdir.h"

