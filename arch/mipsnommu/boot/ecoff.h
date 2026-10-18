/*
 * Some ECOFF definitions.
 *
 * NOTE: the on-disk ECOFF structures use 32 bit fields; the original
 * header used `long', which happened to be 32 bits on the i386 hosts
 * this tool was written for.  Use a fixed width type so the layout is
 * also correct when building the host tool on 64 bit machines.
 */
#include <sys/types.h>

typedef u_int32_t ecoff_long;
typedef struct filehdr {
        unsigned short  f_magic;        /* magic number */
        unsigned short  f_nscns;        /* number of sections */
        ecoff_long            f_timdat;       /* time & date stamp */
        ecoff_long            f_symptr;       /* file pointer to symbolic header */
        ecoff_long            f_nsyms;        /* sizeof(symbolic hdr) */
        unsigned short  f_opthdr;       /* sizeof(optional hdr) */
        unsigned short  f_flags;        /* flags */
} FILHDR;
#define FILHSZ  sizeof(FILHDR)

#define OMAGIC		0407
#define MIPSEBMAGIC	0x160
#define MIPSELMAGIC	0x162

typedef struct scnhdr {
        char            s_name[8];      /* section name */
        ecoff_long            s_paddr;        /* physical address, aliased s_nlib */
        ecoff_long            s_vaddr;        /* virtual address */
        ecoff_long            s_size;         /* section size */
        ecoff_long            s_scnptr;       /* file ptr to raw data for section */
        ecoff_long            s_relptr;       /* file ptr to relocation */
        ecoff_long            s_lnnoptr;      /* file ptr to gp histogram */
        unsigned short  s_nreloc;       /* number of relocation entries */
        unsigned short  s_nlnno;        /* number of gp histogram entries */
        ecoff_long            s_flags;        /* flags */
} SCNHDR;
#define SCNHSZ		sizeof(SCNHDR)
#define SCNROUND	((ecoff_long)16)

typedef struct aouthdr {
        short   magic;          /* see above                            */
        short   vstamp;         /* version stamp                        */
        ecoff_long    tsize;          /* text size in bytes, padded to DW bdry*/
        ecoff_long    dsize;          /* initialized data "  "                */
        ecoff_long    bsize;          /* uninitialized data "   "             */
        ecoff_long    entry;          /* entry pt.                            */
        ecoff_long    text_start;     /* base of text used for this file      */
        ecoff_long    data_start;     /* base of data used for this file      */
        ecoff_long    bss_start;      /* base of bss used for this file       */
        ecoff_long    gprmask;        /* general purpose register mask        */
        ecoff_long    cprmask[4];     /* co-processor register masks          */
        ecoff_long    gp_value;       /* the gp value used for this object    */
} AOUTHDR;
#define AOUTHSZ sizeof(AOUTHDR)

#define OMAGIC		0407
#define NMAGIC		0410
#define ZMAGIC		0413
#define SMAGIC		0411
#define LIBMAGIC        0443

#define N_TXTOFF(f, a) \
 ((a).magic == ZMAGIC || (a).magic == LIBMAGIC ? 0 : \
  ((a).vstamp < 23 ? \
   ((FILHSZ + AOUTHSZ + (f).f_nscns * SCNHSZ + 7) & 0xfffffff8) : \
   ((FILHSZ + AOUTHSZ + (f).f_nscns * SCNHSZ + SCNROUND-1) & ~(SCNROUND-1)) ) )
#define N_DATOFF(f, a) \
  N_TXTOFF(f, a) + (a).tsize;
