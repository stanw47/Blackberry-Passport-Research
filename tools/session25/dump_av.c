#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/avtab.h>
#include <sepol/policydb/hashtab.h>

static policydb_t pdb;
static int mode; /* 1 = target contains "block_device", 2 = source contains "diagnostics" */

static const char *tname(uint16_t v)
{
    if (!v || v > pdb.p_types.nprim || !pdb.p_type_val_to_name)
        return "?";
    return pdb.p_type_val_to_name[v - 1];
}

static const char *cname(uint16_t v)
{
    if (!v || v > pdb.p_classes.nprim || !pdb.p_class_val_to_name)
        return "?";
    return pdb.p_class_val_to_name[v - 1];
}

static int perm_cb(hashtab_key_t key, hashtab_datum_t dat, void *args)
{
    uint32_t mask = *(uint32_t *)args;
    perm_datum_t *pd = (perm_datum_t *)dat;
    if (pd && (mask & (1u << (pd->s.value - 1))))
        printf("%s ", (const char *)key);
    return 0;
}

static int cb(avtab_key_t *k, avtab_datum_t *d, void *args)
{
    (void)args;
    if (!(k->specified & AVTAB_ALLOWED))
        return 0;

    const char *src = tname(k->source_type);
    const char *tgt = tname(k->target_type);

    int show = 0;
    if (mode == 1 && strstr(tgt, "block_device"))
        show = 1;
    if (mode == 2 && strstr(src, "diagnostics"))
        show = 1;
    if (!show)
        return 0;

    printf("%s %s:%s ", src, tgt, cname(k->target_class));
    class_datum_t *cd = pdb.class_val_to_struct[k->target_class - 1];
    if (cd) {
        uint32_t mask = d->data;
        hashtab_map(cd->permissions.table, perm_cb, &mask);
    }
    printf("\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <sepolicy> <block|diag>\n", argv[0]);
        return 1;
    }
    mode = strcmp(argv[2], "block") == 0 ? 1 : 2;

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }

    policy_file_t pf;
    policy_file_init(&pf);
    pf.type = PF_USE_STDIO;
    pf.fp = f;

    if (policydb_read(&pdb, &pf, 0)) {
        fprintf(stderr, "policydb_read failed\n");
        return 1;
    }
    fprintf(stderr, "policy version %u, %u types, %u classes\n",
            pdb.policyvers, pdb.p_types.nprim, pdb.p_classes.nprim);

    avtab_map(&pdb.te_avtab, cb, NULL);
    return 0;
}
