// adc_argp command-line argument parsing library by Anthony Del Ciotto.

#ifndef _ADC_ARGP_H
#define _ADC_ARGP_H_

#include <stdio.h> // For FILE*

#define ADC_ARGP_VERSION "0.3.0"

typedef enum {
        ADC_ARGP_TYPE_HELP,
        ADC_ARGP_TYPE_FLAG,
        ADC_ARGP_TYPE_BOOL,
        ADC_ARGP_TYPE_STRING,
        ADC_ARGP_TYPE_INT,
        ADC_ARGP_TYPE_UINT,
        ADC_ARGP_TYPE_FLOAT,
        ADC_ARGP_TYPE_DOUBLE,
        ADC_ARGP_TYPE_MAX
} adc_argp_type;

typedef struct {
        const char *name;
        const char *shortname;
        adc_argp_type valtype;
        void *val;
        const char *desc;
} adc_argp_option;

#define ADC_ARGP_OPTION(name, sname, type, val, desc)                          \
        (adc_argp_option) {                                                    \
                (name), (sname), (type), (val), (desc)                         \
        }
#define ADC_ARGP_HELP()                                                        \
        ADC_ARGP_OPTION("help", "h", ADC_ARGP_TYPE_HELP, NULL,                 \
                        "Print usage information")

typedef struct adc_argp_parser adc_argp_parser;

#define ADC_ARGP_COUNT(x)                                                      \
        ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

#ifdef __cplusplus
extern "C" {
#endif

// adc_argp_new_parser() - Create a new command line option parser.
// opts - Array of options.
// opts_len - The length of the opts array. Can use the ADC_ARGP_COUNT macro.
adc_argp_parser *adc_argp_new_parser(adc_argp_option *opts,
                                     unsigned int opts_len);

// adc_argp_destroy_parser() - Destroy the given command line parser.
void adc_argp_destroy_parser(adc_argp_parser **parser);

// adc_argp_parse() - Parse the given command line args.
// Returns the number of errors. Will be 0 if there were no errors.
int adc_argp_parse(adc_argp_parser *parser, int argc, const char *argv[]);

// adc_argp_print_errors() - Print any errors encountered during parsing to the
// given FILE stream.
void adc_argp_print_errors(adc_argp_parser *parser, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif // _ADC_ARGP_H_
