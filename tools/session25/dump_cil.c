#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sepol/policydb.h>
#include <sepol/kernel_to_cil.h>
#include <sepol/policydb/policydb.h>

/* libsepol internal wrapper layout (policydb_public.c):
 *   struct sepol_policydb { policydb_t p; };
 */
struct spdb { policydb_t p; };

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <sepolicy>\n", argv[0]); return 1; }

    sepol_policydb_t *sp = NULL;
    sepol_policy_file_t *pf = NULL;
    if (sepol_policydb_create(&sp) || sepol_policy_file_create(&pf)) {
        fprintf(stderr, "create failed\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }

    sepol_policy_file_set_fp(pf, f);
    int rc = sepol_policydb_read(sp, pf);
    if (rc) {
        /* fall back to from_image */
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        void *buf = malloc(len);
        if (!buf || fread(buf, 1, len, f) != (size_t)len) { fprintf(stderr, "read file failed\n"); return 1; }
        sepol_handle_t *h = sepol_handle_create();
        if (!h) { fprintf(stderr, "handle failed\n"); return 1; }
        rc = sepol_policydb_from_image(h, buf, len, sp);
    }
    if (rc) { fprintf(stderr, "policydb read failed (rc=%d)\n", rc); return 1; }

    struct spdb *w = (struct spdb *)sp;
    fprintf(stderr, "policy version %u, %u types, %u classes\n",
            w->p.policyvers, w->p.p_types.nprim, w->p.p_classes.nprim);
    return sepol_kernel_policydb_to_cil(stdout, &w->p);
}
