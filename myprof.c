/*
 * CS 250 Project 5
 * Kian Kasad
 * Fall 2024
 *
 * Copyright (C) 2024 Kian Kasad.
 * Do not redistribute.
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HIST_EXTENSION ".hist"
#define TOP_N          10lu

/* Histogram entry. */
typedef struct hist_entry {
    uintptr_t instr_addr;
    unsigned long ms;
} hist_entry_t;

/* Function specification. */
typedef struct func_spec {
    char *funcname;
    uintptr_t addr_start;
    uintptr_t addr_end; /* Inclusive */
    unsigned long ms;
} func_spec_t;

/* Disassembled instruction specification. */
typedef struct instr_spec {
    uintptr_t addr_start;
    uintptr_t addr_end; /* Inclusive */
} instr_spec_t;

/*
 * Parses a line of output from objdump(1) which contains an instruction and
 * populates an instr_spec_t with the parsed data.
 * Returns true on success and false on failure.
 */
static
bool
parse_disassembly_line(const char *line, instr_spec_t *result)
{
    char *copy = strdup(line);
    char *addr_segment = strtok(copy, "\t");
    char *bytes_segment = strtok(NULL, "\t");
    char *asm_segment = strtok(NULL, "");

    uintptr_t addr_start;
    if (sscanf(addr_segment, " %lx:", &addr_start) != 1) {
        free(copy);
        return false;
    }

    /* Count the number of bytes using a state machine. */
    unsigned char n_bytes = 0;
    const char *whitespace = " \t\n";
    for (char *byte = strtok(bytes_segment, whitespace);
         byte;
         byte = strtok(NULL, whitespace)) {
        n_bytes++;
    }
    free(copy);
    if (n_bytes == 0) {
        return false;
    }

    result->addr_start = addr_start;
    result->addr_end = addr_start + n_bytes - 1;
    return 1;
} /* parse_disassembly_line() */

/*
 * Read histogram file. Returns an array of pointers to hist_entry_t structures.
 * The length of the returned array is returned through the second argument.
 * Returns NULL on error.
 */
static
hist_entry_t **
read_histogram_entries(const char *histfilename, unsigned long *lenptr)
{

    FILE *histfp = fopen(histfilename, "r");
    if (!histfp) {
        fprintf(stderr, "Error: Unable to read file %s: %s\n", histfilename, strerror(errno));
        return NULL;
    }

    /* Read histogram entries. */
    unsigned long hist_entries_size = 256;
    unsigned long n_hist_entries = 0;
    hist_entry_t **hist_entries = calloc(hist_entries_size, sizeof(*hist_entries));
    int fields_read;
    unsigned long instr_start;
    unsigned long ms;
    while ((fields_read = fscanf(histfp, "%lx %lums ", &instr_start, &ms)) != EOF) {
        if (fields_read != 2) {
            fprintf(stderr, "Error: Invalid histogram format\n");
            for (unsigned long i = 0; i < n_hist_entries; i++) {
                free(hist_entries[i]);
            }
            free(hist_entries);
            fclose(histfp);
            return NULL;
        }

        /* Resize array by doubling. */
        if (n_hist_entries == hist_entries_size) {
            hist_entries_size <<= 1;
            hist_entries = realloc(hist_entries, hist_entries_size * sizeof(*hist_entries));
        }

        hist_entries[n_hist_entries] = malloc(sizeof(**hist_entries));
        hist_entries[n_hist_entries]->ms = ms;
        hist_entries[n_hist_entries]->instr_addr = instr_start;
        n_hist_entries++;
    }
    fclose(histfp);
    histfp = NULL;

    *lenptr = n_hist_entries;
    return hist_entries;
} /* read_histogram_entries() */

/*
 * Call nm(1) and get list of functions in the given program. Returns an array
 * of pointers to func_spec_t structures. The length of the returned array is
 * returned through the second argument. Returns NULL on error.
 */
static
func_spec_t **
get_function_list(const char *progname, unsigned long *lenptr)
{
    const char *cmd_fmt = "nm -g -C -S --defined-only --numeric-sort %s > %s";
    char nm_out_filename[] = "/tmp/myprof.XXXXXX";
    mktemp(nm_out_filename);
    size_t cmd_len = snprintf(NULL, 0, cmd_fmt, progname, nm_out_filename);
    char *cmd = malloc(cmd_len + 1);
    sprintf(cmd, cmd_fmt, progname, nm_out_filename);
    int status = system(cmd);
    free(cmd);
    if ((status == -1) || (!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
        fprintf(stderr, "Error: Running nm(1) failed\n");
        return NULL;
    }

    /* Read function list. */
    FILE *nmfp = fopen(nm_out_filename, "r");
    if (!nmfp) {
        fprintf(stderr, "Error: Unable to read file %s: %s\n", nm_out_filename, strerror(errno));
        return NULL;
    }
    unsigned long func_list_size = 256;
    unsigned long n_funcs = 0;
    func_spec_t **func_list = calloc(func_list_size, sizeof(*func_list));
    uintptr_t addr_start;
    unsigned long func_size;
    char symbol_type;
    char funcname[1024];
    int fields_read;
    while ((fields_read = fscanf(nmfp, "%lx %lx %c %1023[^\n]\n", &addr_start, &func_size, &symbol_type, funcname)) != EOF) {
        if (fields_read != 4) {
            /* Some lines have no size, so just ignore them. */
            fscanf(nmfp, "%*[^\n]%*c");
            continue;
        }

        /* If this isn't a function, we don't care. */
        if (symbol_type != 't' && symbol_type != 'T') {
            continue;
        }

        /* Resize array by doubling. */
        if (n_funcs == func_list_size) {
            func_list_size <<= 1;
            func_list = realloc(func_list, func_list_size * sizeof(*func_list));
        }

        func_list[n_funcs] = malloc(sizeof(**func_list));
        func_list[n_funcs]->addr_start = addr_start;
        func_list[n_funcs]->addr_end = addr_start + func_size;
        func_list[n_funcs]->funcname = strdup(funcname);
        n_funcs++;
    }
    fclose(nmfp);
    nmfp = NULL;

    /* Clean up temporary file. */
    unlink(nm_out_filename);

    *lenptr = n_funcs;
    return func_list;
} /* get_function_list() */

/*
 * Print the annotated disassembly for the given function.
 * Returns true on success and false on failure.
 */
static
bool
print_function_disassembly(const char *progname, const func_spec_t *func,
                            const hist_entry_t *const *hist_entries,
                            unsigned long n_hist_entries)
{
    char objdump_out_filename[] = "/tmp/myprof.XXXXXX";
    mktemp(objdump_out_filename);
    const char *cmd_fmt = "objdump -C --disassemble=%s %s | grep -i '^\\s\\+[0-9a-f]\\+:' > %s";
    int cmd_len = snprintf(NULL, 0, cmd_fmt, func->funcname, progname, objdump_out_filename);
    char *cmd = malloc(cmd_len + 1);
    sprintf(cmd, cmd_fmt, func->funcname, progname, objdump_out_filename);
    int status = system(cmd);
    free(cmd);
    if ((status == -1) || (!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
        fprintf(stderr, "Error: Running objdump(1) failed\n");
        return false;
    }

    /* Read objdump output. */
    FILE *odfp = fopen(objdump_out_filename, "r");
    if (!odfp) {
        fprintf(stderr, "Error: Unable to read file %s: %s\n", objdump_out_filename, strerror(errno));
        return false;
    }
    char line[1024];
    while (fgets(line, sizeof(line), odfp) != NULL) {
        /* Skip lines that are too long. */
        if (line[strlen(line) - 1] != '\n') {
            int c;
            do {
                c = fgetc(odfp);
            } while ((c != EOF) && (c != '\n'));
            continue;
        }

        /* Parse line. */
        instr_spec_t instruction;
        if (!parse_disassembly_line(line, &instruction)) {
            /* Skip lines we don't recognize. */
            continue;
        }

        /* Find histogram entries which correspond to this instruction. */
        bool found = false;
        unsigned long ms = 0;
        for (unsigned long i = 0; i < n_hist_entries; i++) {
            const hist_entry_t *entry = hist_entries[i];
            if ((entry->instr_addr >= instruction.addr_start)
                && (entry->instr_addr <= instruction.addr_end)) {
                found = true;
                ms += entry->ms;
            }
        }

        if (found) {
            printf("%10lums %s", ms, line);
        }
    }
    fclose(odfp);
    odfp = NULL;

    /* Clean up temporary file. */
    unlink(objdump_out_filename);

    return true;
} /* print_function_disassembly() */

/*
 * Main program function.
 */
int
main(int argc, const char **argv)
{
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <prog>\n", argv[0]);
        return 1;
    }

    /* Read histogram */
    const char *const progname = argv[1];
    char *histfilename = malloc(strlen(progname) + strlen(HIST_EXTENSION) + 1);
    strcpy(histfilename, progname);
    strcat(histfilename, HIST_EXTENSION);
    unsigned long n_hist_entries;
    hist_entry_t **hist_entries = read_histogram_entries(histfilename, &n_hist_entries);
    free(histfilename);
    if (hist_entries == NULL) {
        return 2;
    }

    /* Call nm(1) to produce function list. */
    unsigned long n_funcs;
    func_spec_t **func_list = get_function_list(progname, &n_funcs);
    if (func_list == NULL) {
        return 2;
    }

    /* Calculate total time for each function and find top N.
     * Also find overall total time. */
    unsigned long total_ms = 0;
    func_spec_t *top_funcs[TOP_N] = {};
    for (unsigned long i = 0; i < n_funcs; i++) {
        func_spec_t *func = func_list[i];

        /* Calculate function total. */
        func->ms = 0;
        for (unsigned long j = 0; j < n_hist_entries; j++) {
            hist_entry_t *entry = hist_entries[j];
            if (entry->instr_addr >= func->addr_start && entry->instr_addr <= func->addr_end) {
                func->ms += entry->ms;
            }
        }
        total_ms += func->ms;

        /* Track top N. */
        for (unsigned long j = 0; j < TOP_N; j++) {
            if (top_funcs[j] == NULL) {
                top_funcs[j] = func;
            } else if (top_funcs[j]->ms < func->ms) {
                for (unsigned long k = TOP_N - 1; k > j; k--) {
                    top_funcs[k] = top_funcs[k - 1];
                }
                top_funcs[j] = func;
                break;
            }
        }
    }

    /* Print top N functions. */
    printf("Top %lu functions:\n\n", TOP_N);
    printf("%-10s%20s%20s%20s\n", "ith", "Function", "Time (ms)", "%");
    for (unsigned long i = 0; i < TOP_N; i++) {
        func_spec_t *func = top_funcs[i];
        double percent = 100.0 * func->ms / total_ms;
        printf("%-10lu%20s%18lums%19.1lf%%\n", i + 1, func->funcname, func->ms, percent);
    }

    /* Print assembly for top N functions. */
    printf("\nTop %lu functions assembly:\n", TOP_N);
    for (unsigned long i = 0; i < TOP_N; i++) {
        func_spec_t *func = top_funcs[i];
        double percent = 100.0 * func->ms / total_ms;

        printf("\n%lu:  %-20s%10lums%10.2lf%%\n\n", i + 1, func->funcname, func->ms, percent);

        /* Call objdump(1) to disassemble function. */
        print_function_disassembly(progname, func, (const hist_entry_t *const *) hist_entries,
                                   n_hist_entries);
    }

    /* Free arrays. */
    for (unsigned long i = 0; i < n_hist_entries; i++) {
        free(hist_entries[i]);
    }
    free(hist_entries);
    for (unsigned long i = 0; i < n_funcs; i++) {
        free(func_list[i]->funcname);
        free(func_list[i]);
    }
    free(func_list);

    return 0;
} /* main() */

/* vim: set ts=4 sw=4 et tw=80 : */
